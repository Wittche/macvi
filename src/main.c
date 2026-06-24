/**
 * @file main.c
 * @brief MacWI entry point — parse and display PE file information.
 *
 * Usage:  macwi <path-to-exe>
 *
 * Loads a PE32 executable, prints header information (entry point, sections,
 * data directories), validates the image, and exits cleanly.  No actual
 * execution of the PE code is performed in this initial version.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/pe.h"
#include "macwi/types.h"
#include "macwi/emu.h"
#include "macwi/thunk.h"
#include "win32/kernel32.h"
#include "win32/ntdll.h"
#include "win32/user32.h"
#include "macwi/cocoa_bridge.h"
#include "macwi/gdi32.h"
#include "macwi/vfs.h"
#include "macwi/handle.h"
#include "macwi/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>

HANDLE_TABLE g_macwi_handle_table;

/* ============================================================================
 * Threading
 * ============================================================================ */

typedef struct {
    EMU_CONTEXT* ctx;
    uint32_t entry_point;
    uint32_t esp;
    PE_IMAGE image;
} EmuThreadArgs;

static void* emu_thread_func(void* arg) {
    EmuThreadArgs* args = (EmuThreadArgs*)arg;
    
    printf("\n>>> Starting execution at EIP = 0x%08X <<<\n\n", args->entry_point);
    macwi_status_t status = macwi_emu_start(args->ctx, args->entry_point, args->esp);
    
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

/**
 * Return a human-readable name for a PE machine type.
 */
static const char* internal_machine_name(WORD machine) {
    switch (machine) {
        case IMAGE_FILE_MACHINE_I386:  return "x86 (i386)";
        case IMAGE_FILE_MACHINE_AMD64: return "x86-64 (AMD64)";
        case IMAGE_FILE_MACHINE_ARM64: return "ARM64 (AArch64)";
        default:                       return "Unknown";
    }
}

/**
 * Return a human-readable name for a PE subsystem value.
 */
static const char* internal_subsystem_name(WORD subsystem) {
    switch (subsystem) {
        case 1:  return "Native";
        case 2:  return "Windows GUI";
        case 3:  return "Windows Console (CUI)";
        case 5:  return "OS/2 Console";
        case 7:  return "POSIX Console";
        case 9:  return "Windows CE GUI";
        case 10: return "EFI Application";
        default: return "Unknown";
    }
}

/**
 * Print section characteristic flags in human-readable form.
 */
static void internal_print_section_flags(DWORD chars) {
    if (chars & IMAGE_SCN_CNT_CODE)              printf("CODE ");
    if (chars & IMAGE_SCN_CNT_INITIALIZED_DATA)  printf("IDATA ");
    if (chars & IMAGE_SCN_CNT_UNINITIALIZED_DATA) printf("UDATA ");
    if (chars & IMAGE_SCN_MEM_READ)              printf("R");
    if (chars & IMAGE_SCN_MEM_WRITE)             printf("W");
    if (chars & IMAGE_SCN_MEM_EXECUTE)           printf("X");
}

/* ============================================================================
 * main
 * ============================================================================ */

int main(int argc, char* argv[]) {
    /* ---- Argument parsing ----------------------------------------------- */

    if (argc < 2) {
        fprintf(stderr,
            "MacWI — WoW64-like compatibility layer for Apple Silicon macOS\n"
            "\n"
            "Usage: %s <path-to-exe>\n"
            "\n"
            "Loads and inspects a 32-bit Windows PE executable.\n",
            argv[0]);
        return 1;
    }

    const char* pe_path = argv[1];

    printf("========================================\n");
    printf(" MacWI PE Loader v0.1.0\n");
    printf("========================================\n\n");
    printf("Loading: %s\n\n", pe_path);

    /* ---- Load the PE file ----------------------------------------------- */

    PE_IMAGE image;
    macwi_status_t status = macwi_pe_load_file(pe_path, &image);

    if (status != MACWI_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to load PE file (status=%d)\n", status);
        return 2;
    }

    /* ---- Display PE header information ---------------------------------- */

    const PE_FILE_HEADER* fh = &image.nt_headers->FileHeader;
    const PE_OPTIONAL_HEADER_32* opt = &image.nt_headers->OptionalHeader;

    printf("--- PE File Header ---\n");
    printf("  Machine:            0x%04X (%s)\n", fh->Machine,
           internal_machine_name(fh->Machine));
    printf("  Number of Sections: %u\n", fh->NumberOfSections);
    printf("  Timestamp:          0x%08X\n", fh->TimeDateStamp);
    printf("  Characteristics:    0x%04X\n", fh->Characteristics);
    printf("\n");

    printf("--- PE Optional Header (PE32) ---\n");
    printf("  Magic:              0x%04X\n", opt->Magic);
    printf("  Entry Point (RVA):  0x%08X\n", opt->AddressOfEntryPoint);
    printf("  Entry Point (VA):   0x%08X\n", image.entry_point);
    printf("  Image Base:         0x%08X\n", opt->ImageBase);
    printf("  Section Alignment:  0x%08X\n", opt->SectionAlignment);
    printf("  File Alignment:     0x%08X\n", opt->FileAlignment);
    printf("  Size of Image:      0x%08X (%u bytes)\n",
           opt->SizeOfImage, opt->SizeOfImage);
    printf("  Size of Headers:    0x%08X\n", opt->SizeOfHeaders);
    printf("  Subsystem:          %u (%s)\n", opt->Subsystem,
           internal_subsystem_name(opt->Subsystem));
    printf("  Data Directories:   %u\n", opt->NumberOfRvaAndSizes);
    printf("\n");

    /* ---- Display data directories --------------------------------------- */

    static const char* dir_names[] = {
        "Export", "Import", "Resource", "Exception",
        "Security", "BaseReloc", "Debug", "Architecture",
        "GlobalPtr", "TLS", "LoadConfig", "BoundImport",
        "IAT", "DelayImport", "CLR", "Reserved"
    };

    printf("--- Data Directories ---\n");
    uint32_t num_dirs = opt->NumberOfRvaAndSizes;
    if (num_dirs > PE_MAX_DATA_DIRECTORIES) num_dirs = PE_MAX_DATA_DIRECTORIES;

    for (uint32_t i = 0; i < num_dirs; i++) {
        if (opt->DataDirectory[i].VirtualAddress == 0 &&
            opt->DataDirectory[i].Size == 0) {
            continue;  /* Skip empty entries */
        }
        printf("  [%2u] %-12s  RVA=0x%08X  Size=0x%08X\n",
               i, (i < 16) ? dir_names[i] : "???",
               opt->DataDirectory[i].VirtualAddress,
               opt->DataDirectory[i].Size);
    }
    printf("\n");

    /* ---- Display sections ----------------------------------------------- */

    printf("--- Sections (%u) ---\n", image.num_sections);
    printf("  %-8s  %10s  %10s  %10s  %10s  Flags\n",
           "Name", "VirtAddr", "VirtSize", "RawData", "RawSize");

    for (uint16_t i = 0; i < image.num_sections; i++) {
        const PE_SECTION_HEADER* sec = &image.section_headers[i];

        /* Section name may not be NUL-terminated — print safely */
        char name[PE_SECTION_NAME_SIZE + 1];
        memcpy(name, sec->Name, PE_SECTION_NAME_SIZE);
        name[PE_SECTION_NAME_SIZE] = '\0';

        printf("  %-8s  0x%08X  0x%08X  0x%08X  0x%08X  ",
               name,
               sec->VirtualAddress,
               sec->VirtualSize,
               sec->PointerToRawData,
               sec->SizeOfRawData);
        internal_print_section_flags(sec->Characteristics);
        printf("\n");
    }
    printf("\n");

    /* ---- Validate ------------------------------------------------------- */

    printf("--- Validation ---\n");
    status = macwi_pe_validate(&image);
    if (status == MACWI_SUCCESS) {
        printf("  ✓ PE image is structurally valid.\n");
    } else {
        printf("  ✗ Validation failed (status=%d). See stderr for details.\n",
               status);
    }
    printf("\n");

    /* ---- Import resolution ---------------------------------------------- */

    printf("--- Execution Setup ---\n");

    EMU_CONTEXT* ctx = NULL;
    status = macwi_emu_init(&ctx);
    if (status != MACWI_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to initialize emulator (status=%d)\n", status);
        macwi_pe_free(&image);
        return 3;
    }

    if (macwi_handle_table_init(&g_macwi_handle_table) != MACWI_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to initialize Handle Table\n");
        return 4;
    }

    if (macwi_vfs_init() != MACWI_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to initialize VFS\n");
        macwi_pe_free(&image);
        return 5;
    }

    if (macwi_registry_init() != MACWI_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to initialize Registry\n");
        macwi_pe_free(&image);
        return 6;
    }

   // Temporary declarations for DLL initialization
extern void macwi_kernel32_register_apis(void);
extern void macwi_ntdll_register_apis(void);
extern void macwi_user32_register_apis(void);
extern void macwi_gdi32_register_apis(void);
extern void macwi_advapi32_register_apis(void);

    macwi_thunk_init_dispatcher(ctx);
    macwi_kernel32_register_apis();
    macwi_ntdll_register_apis();
    macwi_user32_register_apis();
    macwi_gdi32_register_apis();
    macwi_advapi32_register_apis();
    printf("  [+] Registered APIs and Dispatcher\n");

    status = macwi_pe_resolve_imports(&image);
    if (status == MACWI_SUCCESS) {
        printf("  [+] IAT patched with Magic VAs\n");
    } else {
        printf("  [-] Import resolution skipped or failed (status=%d).\n", status);
    }

    status = macwi_pe_map_to_emu(&image, ctx);
    if (status != MACWI_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to map PE to emulator (status=%d)\n", status);
        macwi_emu_free(ctx);
        macwi_pe_free(&image);
        return 4;
    }
    printf("  [+] PE mapped into Emulator memory\n");

    /* ---- Stack Setup and Execution -------------------------------------- */

    uint32_t stack_base = 0x80000000;
    uint32_t stack_size = 0x100000; // 1MB
    status = macwi_emu_map_memory(ctx, stack_base, stack_size, MACWI_PROT_READ | MACWI_PROT_WRITE);
    if (status == MACWI_SUCCESS) {
        printf("  [+] Allocated 1MB stack at 0x%08X\n", stack_base);
    }

    uint32_t esp = stack_base + stack_size - 4;
    uint32_t dummy_ret = 0xDEADBEEF; // Will cause exit when the entry point returns
    macwi_emu_write_memory(ctx, esp, &dummy_ret, 4);

    macwi_cocoa_init();
    printf("  [+] Initialized Cocoa Bridge\n");

    EmuThreadArgs* args = malloc(sizeof(EmuThreadArgs));
    args->ctx = ctx;
    args->entry_point = image.entry_point;
    args->esp = esp;
    args->image = image;

    pthread_t emu_thread;
    pthread_create(&emu_thread, NULL, emu_thread_func, args);

    // Block main thread with Cocoa event loop
    macwi_cocoa_run_loop();

    /* ---- Cleanup (reached if Cocoa loop exits) -------------------------- */

    macwi_emu_free(ctx);
    macwi_pe_free(&image);

    printf("\nDone.\n");
    return 0;
}
