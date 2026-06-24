/**
 * @file pe_loader.c
 * @brief PE file loader — file I/O and section mapping.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/pe.h"
#include "macwi/emu.h"
#include "macwi/thunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* ============================================================================
 * macwi_pe_load_file
 * ============================================================================ */

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

    uint32_t image_size = out_image->nt_headers->OptionalHeader.SizeOfImage;
    void* image_base = malloc(image_size);
    if (!image_base) {
        free(file_data);
        memset(out_image, 0, sizeof(PE_IMAGE));
        return MACWI_ERROR_MEMORY;
    }
    memset(image_base, 0, image_size);

    /* Copy headers */
    uint32_t headers_size = out_image->nt_headers->OptionalHeader.SizeOfHeaders;
    if (headers_size > total_read) headers_size = (uint32_t)total_read;
    memcpy(image_base, file_data, headers_size);

    /* Copy sections */
    size_t pe_off = out_image->dos_header->e_lfanew;
    const PE_SECTION_HEADER* sec = (const PE_SECTION_HEADER*)(
        file_data + pe_off + sizeof(DWORD) + sizeof(PE_FILE_HEADER) + 
        out_image->nt_headers->FileHeader.SizeOfOptionalHeader);

    for (int i = 0; i < out_image->nt_headers->FileHeader.NumberOfSections; i++) {
        if (sec[i].SizeOfRawData == 0) continue;
        uint32_t sec_rva = sec[i].VirtualAddress;
        uint32_t raw_size = sec[i].SizeOfRawData;
        uint32_t raw_ptr = sec[i].PointerToRawData;
        uint32_t copy_size = raw_size;
        
        if (sec[i].VirtualSize > 0 && raw_size > sec[i].VirtualSize) {
            copy_size = sec[i].VirtualSize;
        }

        if (raw_ptr + copy_size <= file_size && sec_rva + copy_size <= image_size) {
            memcpy((uint8_t*)image_base + sec_rva, file_data + raw_ptr, copy_size);
        }
    }

    out_image->mapped_base = image_base;
    out_image->mapped_size = image_size;
    out_image->is_loaded = true;
    out_image->file_path = strdup(path);

    /* Update pointers */
    out_image->dos_header = (const DOS_HEADER*)image_base;
    out_image->nt_headers = (const IMAGE_NT_HEADERS_32*)((uint8_t*)image_base + pe_off);
    out_image->section_headers = (const PE_SECTION_HEADER*)(
        (uint8_t*)image_base + pe_off + sizeof(DWORD) +
        sizeof(PE_FILE_HEADER) + out_image->nt_headers->FileHeader.SizeOfOptionalHeader);

    free(file_data);
    return MACWI_SUCCESS;
}

uint32_t macwi_pe_get_entry_point(const PE_IMAGE* image) {
    if (!image || !image->nt_headers) return 0;
    return image->nt_headers->OptionalHeader.ImageBase +
           image->nt_headers->OptionalHeader.AddressOfEntryPoint;
}

macwi_status_t macwi_pe_resolve_imports(PE_IMAGE* image) {
    if (!image || !image->is_loaded) return MACWI_ERROR_INVALID_PARAM;

    uint32_t import_va = image->nt_headers->OptionalHeader.DataDirectory[1].VirtualAddress; // IMAGE_DIRECTORY_ENTRY_IMPORT
    if (import_va == 0) return MACWI_SUCCESS; // No imports

    PE_IMPORT_DESCRIPTOR* import_desc = (PE_IMPORT_DESCRIPTOR*)((uint8_t*)image->mapped_base + import_va);

    while (import_desc->Name != 0) {
        char* dll_name = (char*)image->mapped_base + import_desc->Name;
        
        uint32_t thunk_rva = import_desc->FirstThunk;
        uint32_t orig_thunk_rva = import_desc->OriginalFirstThunk;
        if (orig_thunk_rva == 0) orig_thunk_rva = thunk_rva; // Borland compiler sometimes does this

        uint32_t* thunk = (uint32_t*)((uint8_t*)image->mapped_base + thunk_rva);
        uint32_t* orig_thunk = (uint32_t*)((uint8_t*)image->mapped_base + orig_thunk_rva);

        while (*orig_thunk != 0) {
            if (*orig_thunk & 0x80000000) {
                // Ordinal import
                // fprintf(stderr, "Ordinal import not supported yet\n");
            } else {
                // Name import
                // *orig_thunk is an RVA to an IMAGE_IMPORT_BY_NAME struct (Hint: WORD, Name: ASCIIZ)
                char* func_name = (char*)image->mapped_base + *orig_thunk + 2;
                
                // Get Magic VA for this API
                uint32_t magic_va = macwi_thunk_get_magic_va(dll_name, func_name);
                if (magic_va == 0) {
                    fprintf(stderr, "[macwi_loader] WARNING: Unresolved import %s!%s\n", dll_name, func_name);
                    // Just write a breakpoint (0xCC) or keep original
                } else {
                    // Overwrite the IAT entry with our magic VA
                    *thunk = magic_va;
                }
            }
            thunk++;
            orig_thunk++;
        }
        import_desc++;
    }

    return MACWI_SUCCESS;
}

void macwi_pe_free(PE_IMAGE* image) {
    if (!image) return;
    if (image->is_loaded && image->mapped_base) {
        free(image->mapped_base);
    }
    if (image->file_path) {
        free((void*)image->file_path);
    }
    memset(image, 0, sizeof(PE_IMAGE));
}

macwi_status_t macwi_pe_map_to_emu(const PE_IMAGE* image, EMU_CONTEXT* emu_ctx) {
    if (!image || !image->is_loaded || !emu_ctx) return MACWI_ERROR_INVALID_PARAM;

    uint32_t image_base = image->nt_headers->OptionalHeader.ImageBase;
    uint32_t image_size = image->nt_headers->OptionalHeader.SizeOfImage;

    macwi_status_t status = macwi_emu_map_memory(emu_ctx, image_base, image_size, MACWI_PROT_ALL);
    if (status != MACWI_SUCCESS) return status;

    status = macwi_emu_write_memory(emu_ctx, image_base, image->mapped_base, image_size);
    if (status != MACWI_SUCCESS) return status;

    return MACWI_SUCCESS;
}
