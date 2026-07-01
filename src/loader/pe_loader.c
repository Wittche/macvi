/**
 * @file pe_loader.c
 * @brief PE file loader — file I/O and section mapping for PE32/PE32+.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/pe.h"
#include "macwi/emu.h"
#include "macwi/pe.h"
#include "macwi/thunk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

macwi_status_t macwi_pe_load_file(const char* path, PE_IMAGE* out_image) {
    if (!path || !out_image) return MACWI_ERROR_INVALID_PARAM;
    memset(out_image, 0, sizeof(PE_IMAGE));

    int fd = open(path, O_RDONLY);
    if (fd < 0) return MACWI_ERROR_IO;

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return MACWI_ERROR_IO;
    }

    size_t file_size = (size_t)st.st_size;
    if (file_size < sizeof(DOS_HEADER)) {
        close(fd);
        return MACWI_ERROR_INVALID_PE;
    }

    uint8_t* file_data = (uint8_t*)malloc(file_size);
    if (!file_data) {
        close(fd);
        return MACWI_ERROR_MEMORY;
    }

    size_t total_read = 0;
    while (total_read < file_size) {
        ssize_t n = read(fd, file_data + total_read, file_size - total_read);
        if (n < 0) {
            if (errno == EINTR) continue;
            free(file_data);
            close(fd);
            return MACWI_ERROR_IO;
        }
        if (n == 0) break;
        total_read += (size_t)n;
    }
    close(fd);

    macwi_status_t status = macwi_pe_parse_headers(file_data, total_read, out_image);
    if (status != MACWI_SUCCESS) {
        free(file_data);
        return status;
    }

    uint32_t image_size = out_image->size_of_image;
    // Align to 4KB (0x1000)
    image_size = (image_size + 0xFFF) & ~0xFFF;
    
    // Use mmap so FEX can easily access it if needed, or we just malloc. 
    // FEX might require it to be mapped in guest address space, but FEX handles memory via FEX_MapMemory.
    // For now, we malloc and will copy to FEX via macwi_emu_write_memory. Wait!
    // If we use FEX_MapMemory, we don't need to malloc here.
    // However, macwi_pe_load_file does NOT map into emu. `main.c` does that by calling `macwi_emu_map_memory`!
    // So this `image_base` is just host backing store before it gets mapped into EMU_CONTEXT.
    
    void* image_base = malloc(image_size);
    if (!image_base) {
        free(file_data);
        memset(out_image, 0, sizeof(PE_IMAGE));
        return MACWI_ERROR_MEMORY;
    }
    memset(image_base, 0, image_size);

    uint32_t SizeOfHeaders = out_image->is_64bit ? 
        out_image->optional_header.opt_64->SizeOfHeaders : 
        out_image->optional_header.opt_32->SizeOfHeaders;

    uint32_t headers_size = SizeOfHeaders;
    if (headers_size > total_read) headers_size = (uint32_t)total_read;
    memcpy(image_base, file_data, headers_size);

    for (int i = 0; i < out_image->num_sections; i++) {
        const PE_SECTION_HEADER* sec = &out_image->section_headers[i];
        uint32_t sec_rva = sec->VirtualAddress;
        
        if (sec->SizeOfRawData == 0) {
            // BSS-like section: zero-fill the virtual area
            if (sec->VirtualSize > 0 && sec_rva + sec->VirtualSize <= image_size) {
                memset((uint8_t*)image_base + sec_rva, 0, sec->VirtualSize);
            }
            continue;
        }
        
        uint32_t raw_size = sec->SizeOfRawData;
        uint32_t raw_ptr = sec->PointerToRawData;
        uint32_t copy_size = raw_size;
        
        if (sec->VirtualSize > 0 && raw_size > sec->VirtualSize) {
            copy_size = sec->VirtualSize;
        }

        if (raw_ptr + copy_size <= total_read && sec_rva + copy_size <= image_size) {
            memcpy((uint8_t*)image_base + sec_rva, file_data + raw_ptr, copy_size);
        }
        
        // Zero-fill remaining virtual space if VirtualSize > SizeOfRawData
        if (sec->VirtualSize > copy_size && sec_rva + sec->VirtualSize <= image_size) {
            memset((uint8_t*)image_base + sec_rva + copy_size, 0, sec->VirtualSize - copy_size);
        }
    }

    out_image->mapped_base = (uint8_t*)image_base;
    out_image->mapped_size = image_size;
    out_image->file_path = strdup(path);

    // Update pointers to point to new image_base
    size_t pe_offset = out_image->dos_header->e_lfanew;
    const DOS_HEADER* dos = (const DOS_HEADER*)image_base;
    out_image->dos_header = dos;
    
    // Re-parse from the mapped base to get valid pointers
    free(file_data); // Free the raw file data since we copied it into image_base
    return macwi_pe_parse_headers((const uint8_t*)image_base, image_size, out_image);
}

void macwi_pe_free(PE_IMAGE* image) {
    if (!image) return;
    if (image->mapped_base) {
        free(image->mapped_base);
    }
    if (image->file_path) {
        free((void*)image->file_path);
    }
    memset(image, 0, sizeof(PE_IMAGE));
}

uint64_t macwi_pe_get_entry_point(const PE_IMAGE* image) {
    if (!image) return 0;
    return image->entry_point;
}

macwi_status_t macwi_pe_resolve_imports(PE_IMAGE* image, struct EMU_CONTEXT* ctx) {
    if (!image || !image->mapped_base || !ctx) return MACWI_ERROR_INVALID_PARAM;
    
    PE_DATA_DIRECTORY* import_dir = NULL;
    if (image->is_64bit) {
        import_dir = (PE_DATA_DIRECTORY*)&image->optional_header.opt_64->DataDirectory[1];
    } else {
        import_dir = (PE_DATA_DIRECTORY*)&image->optional_header.opt_32->DataDirectory[1];
    }
    
    if (import_dir->VirtualAddress == 0 || import_dir->Size == 0) {
        printf("[macwi:loader] No imports found.\n");
        return MACWI_SUCCESS; // No imports
    }
    
    PE_IMPORT_DESCRIPTOR* import_desc = (PE_IMPORT_DESCRIPTOR*)((uint8_t*)image->mapped_base + import_dir->VirtualAddress);
    
    while (import_desc->Name != 0) {
        const char* dll_name = (const char*)((uint8_t*)image->mapped_base + import_desc->Name);
        printf("[macwi:loader] Resolving imports for %s...\n", dll_name);
        
        uint32_t thunk_rva = import_desc->FirstThunk;
        uint32_t orig_thunk_rva = import_desc->OriginalFirstThunk;
        if (orig_thunk_rva == 0) orig_thunk_rva = thunk_rva;
        
        if (image->is_64bit) {
            uint64_t* thunk = (uint64_t*)((uint8_t*)image->mapped_base + thunk_rva);
            uint64_t* orig_thunk = (uint64_t*)((uint8_t*)image->mapped_base + orig_thunk_rva);
            
            while (*orig_thunk != 0) {
                if (!(*orig_thunk & 0x8000000000000000ULL)) { // Not ordinal
                    uint32_t name_rva = (uint32_t)(*orig_thunk & 0xFFFFFFFF);
                    const char* func_name = (const char*)((uint8_t*)image->mapped_base + name_rva + 2);
                    
                    uint64_t tramp_va = macwi_thunk_get_trampoline(ctx, dll_name, func_name);
                    if (tramp_va != 0) {
                        *thunk = tramp_va;
                    } else {
                        fprintf(stderr, "[macwi:loader] Warning: Unresolved import %s!%s\n", dll_name, func_name);
                    }
                }
                thunk++;
                orig_thunk++;
            }
        } else {
            uint32_t* thunk = (uint32_t*)((uint8_t*)image->mapped_base + thunk_rva);
            uint32_t* orig_thunk = (uint32_t*)((uint8_t*)image->mapped_base + orig_thunk_rva);
            
            while (*orig_thunk != 0) {
                if (!(*orig_thunk & 0x80000000)) { // Not ordinal
                    uint32_t name_rva = *orig_thunk;
                    const char* func_name = (const char*)((uint8_t*)image->mapped_base + name_rva + 2);
                    
                    uint64_t tramp_va = macwi_thunk_get_trampoline(ctx, dll_name, func_name);
                    if (tramp_va != 0) {
                        *thunk = (uint32_t)tramp_va;
                    } else {
                        fprintf(stderr, "[macwi:loader] Warning: Unresolved import %s!%s\n", dll_name, func_name);
                    }
                }
                thunk++;
                orig_thunk++;
            }
        }
        import_desc++;
    }
    
    return MACWI_SUCCESS;
}
