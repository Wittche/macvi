/**
 * @file main.c
 * @brief MacWI entry point — parse and display PE file information.
 *
 * Usage:  macwi <path-to-exe>
 *
 * Loads a PE32+ executable, prints header information, initializes FEXCore,
 * maps the PE into memory, and starts execution.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/pe.h"
#include "macwi/types.h"
#include "macwi/emu.h"
#include "macwi/thunk.h"
#include "win32/kernel32.h"
#include "win32/ntdll.h"
#include "macwi/vfs.h"
#include "macwi/handle.h"
#include "macwi/registry.h"
#include "macwi/thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>

HANDLE_TABLE g_macwi_handle_table;

/* ============================================================================
 * Threading
 * ============================================================================ */

typedef struct {
    EMU_CONTEXT* ctx;
    uint64_t entry_point;
    uint64_t rsp;
    PE_IMAGE image;
} EmuThreadArgs;

static void* emu_thread_func(void* arg) {
    EmuThreadArgs* args = (EmuThreadArgs*)arg;
    
    printf("\n>>> Starting execution at RIP = 0x%016llX <<<\n\n", args->entry_point);
    macwi_emu_set_pc(args->ctx, args->entry_point);
    macwi_emu_set_sp(args->ctx, args->rsp);

    macwi_status_t status = macwi_emu_start(args->ctx);
    
    if (status == MACWI_SUCCESS) {
        printf("\n>>> Execution completed gracefully. <<<\n");
    } else {
        printf("\n>>> Execution stopped (status=%d) <<<\n", status);
    }
    
    // Cleanup and exit the whole process when emulation finishes
    macwi_emu_free(args->ctx);
    exit(0);
    return NULL;
}

/* ============================================================================
 * Helpers for pretty-printing
 * ============================================================================ */

static const char* internal_machine_name(WORD machine) {
    switch (machine) {
        case IMAGE_FILE_MACHINE_I386:  return "x86 (i386)";
        case IMAGE_FILE_MACHINE_AMD64: return "x86-64 (AMD64)";
        case IMAGE_FILE_MACHINE_ARM64: return "ARM64 (AArch64)";
        default:                       return "Unknown";
    }
}

static const char* internal_subsystem_name(WORD subsystem) {
    switch (subsystem) {
        case 1:  return "Native";
        case 2:  return "Windows GUI";
        case 3:  return "Windows Console (CUI)";
        default: return "Unknown";
    }
}

static void internal_print_section_flags(DWORD chars) {
    if (chars & IMAGE_SCN_CNT_CODE)              printf("CODE ");
    if (chars & IMAGE_SCN_CNT_INITIALIZED_DATA)  printf("IDATA ");
    if (chars & IMAGE_SCN_CNT_UNINITIALIZED_DATA)printf("UDATA ");
    if (chars & IMAGE_SCN_MEM_EXECUTE)           printf("EXEC ");
    if (chars & IMAGE_SCN_MEM_READ)              printf("READ ");
    if (chars & IMAGE_SCN_MEM_WRITE)             printf("WRITE ");
}

/* ============================================================================
 * Main Program
 * ============================================================================ */

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path-to-exe>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* exe_path = argv[1];
    PE_IMAGE image;

    // 1. Initialize Subsystems
    printf("[macwi] Initializing Virtual File System...\n");
    macwi_vfs_init();
    
    printf("[macwi] Initializing Registry Virtualization...\n");
    macwi_registry_init();
    
    printf("[macwi] Initializing Handle Table...\n");
    macwi_handle_table_init(&g_macwi_handle_table);
    
    // 2. Load PE File
    printf("[macwi] Loading PE file: %s\n", exe_path);
    macwi_status_t status = macwi_pe_load_file(exe_path, &image);
    if (status != MACWI_SUCCESS) {
        fprintf(stderr, "[macwi] Failed to load PE file (status = %d)\n", status);
        return EXIT_FAILURE;
    }

    // 3. Print Image Details
    printf("\n=== PE File Information ===\n");
    printf("Machine:        %s (0x%04X)\n", 
           internal_machine_name(image.file_header->Machine), 
           image.file_header->Machine);
    
    WORD subsystem = image.is_64bit ? image.optional_header.opt_64->Subsystem : image.optional_header.opt_32->Subsystem;
    
    printf("Subsystem:      %s (0x%04X)\n", internal_subsystem_name(subsystem), subsystem);
    printf("Entry Point:    0x%016llX\n", image.entry_point);
    printf("Image Base:     0x%016llX\n", image.image_base);
    printf("Image Size:     0x%08X (%u bytes)\n", image.size_of_image, image.size_of_image);
    printf("Sections:       %u\n", image.num_sections);

    printf("\n--- Section Table ---\n");
    for (uint16_t i = 0; i < image.num_sections; i++) {
        const PE_SECTION_HEADER* sec = &image.section_headers[i];
        
        char name[PE_SECTION_NAME_SIZE + 1];
        memcpy(name, sec->Name, PE_SECTION_NAME_SIZE);
        name[PE_SECTION_NAME_SIZE] = '\0';

        printf("  [%2u] %-8s RVA: 0x%08X  VSize: 0x%08X  RawSize: 0x%08X  Flags: ",
               i + 1, name, sec->VirtualAddress, sec->VirtualSize, sec->SizeOfRawData);
        internal_print_section_flags(sec->Characteristics);
        printf("\n");
    }

    // 4. Initialize FEXCore Emulator
    printf("\n[macwi] Initializing FEXCore x86_64 Emulator...\n");
    EMU_CONTEXT* ctx = NULL;
    if (macwi_emu_init(&ctx) != MACWI_SUCCESS) {
        fprintf(stderr, "[macwi] Failed to initialize FEXCore Emulator\n");
        return EXIT_FAILURE;
    }

    // 5. Initialize Win32 APIs and Thunking
    macwi_kernel32_register_apis();
    macwi_ntdll_register_apis();
    macwi_thunk_init_dispatcher(ctx);

    // 6. Map PE into FEXCore Memory
    printf("[macwi] Mapping PE into Emulator Memory...\n");
    if (macwi_emu_map_memory(ctx, image.image_base, image.size_of_image, MACWI_PROT_ALL, NULL) != MACWI_SUCCESS) {
        fprintf(stderr, "[macwi] Failed to map PE base memory\n");
        return EXIT_FAILURE;
    }
    if (macwi_emu_write_memory(ctx, image.image_base, image.mapped_base, image.size_of_image) != MACWI_SUCCESS) {
        fprintf(stderr, "[macwi] Failed to write PE sections to Emulator Memory\n");
        return EXIT_FAILURE;
    }

    // 7. Allocate Guest Stack (2MB)
    uint64_t stack_base = 0x7FFF00000000ULL;
    uint32_t stack_size = 2 * 1024 * 1024;
    printf("[macwi] Allocating Guest Stack (2MB) at 0x%016llX...\n", stack_base);
    if (macwi_emu_map_memory(ctx, stack_base, stack_size, MACWI_PROT_READ | MACWI_PROT_WRITE, NULL) != MACWI_SUCCESS) {
        fprintf(stderr, "[macwi] Failed to map Guest Stack\n");
        return EXIT_FAILURE;
    }
    uint64_t rsp_top = stack_base + stack_size - 0x1000; // Leave some space at the top

    // 8. Launch execution thread
    EmuThreadArgs* args = malloc(sizeof(EmuThreadArgs));
    args->ctx = ctx;
    args->entry_point = image.entry_point;
    args->rsp = rsp_top;
    args->image = image;

    pthread_t thread;
    pthread_create(&thread, NULL, emu_thread_func, args);

    // Provide a simple loop to keep the main thread alive and optionally process GUI
    printf("[macwi] Emulator thread detached. Main thread idling...\n");
    
    // In a real GUI app, we'd start the Cocoa event loop here
    while(1) {
        sleep(1);
    }

    // Unreachable, emu thread calls exit()
    macwi_pe_free(&image);
    macwi_emu_free(ctx);
    macwi_handle_table_destroy(&g_macwi_handle_table);
    return EXIT_SUCCESS;
}
