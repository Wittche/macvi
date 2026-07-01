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
#include "win32/advapi32.h"
#include "win32/cocoa_window.h"

void macwi_cocoa_run_loop(void);

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
#include <signal.h>
#define _XOPEN_SOURCE 700
#include <ucontext.h>

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

static EMU_CONTEXT* g_current_emu_ctx = NULL;

static void host_signal_handler(int sig, siginfo_t* info, void* ucontext) {
    fprintf(stderr, "\n=========================================\n");
    fprintf(stderr, "[macwi] FATAL: Host received signal %d (Address: %p)\n", sig, info->si_addr);
    
    if (g_current_emu_ctx) {
        uint64_t global_base = macwi_emu_get_global_memory_base();
        
#if defined(__aarch64__) || defined(_M_ARM_64)
        ucontext_t* uc = (ucontext_t*)ucontext;
#ifdef __APPLE__
        uintptr_t host_pc = uc->uc_mcontext->__ss.__pc;
#else
        uintptr_t host_pc = uc->uc_mcontext.pc;
#endif
#endif

        // In FEXCore JIT, host_pc doesn't neatly map to Guest RIP directly,
        // but we know that if we had an actual exception, FEXCore pushes it to info->si_addr.
        // Or if it was a direct native crash in jitted code, si_addr might be the faulting address.
        // We will just read macwi_emu_get_pc safely if possible, or fallback.
        // Actually, FEXCore's ThreadSetReg is safe to call if we don't access CurrentFrame.
        uint64_t rip = macwi_emu_get_pc(g_current_emu_ctx);
        uint64_t fault_addr = (uint64_t)info->si_addr;
        if (fault_addr >= global_base) {
            fault_addr -= global_base;
        }

        fprintf(stderr, "[macwi] Guest RIP at time of crash: 0x%016llX (Fault addr: 0x%llX)\n", rip, fault_addr);
        
        // Full SEH: Construct EXCEPTION_RECORD on the guest stack and redirect to KiUserExceptionDispatcher
        abort();
        uint64_t rsp = macwi_emu_get_sp(g_current_emu_ctx);
        
        // Allocate 80 bytes for 32-bit EXCEPTION_RECORD
        rsp -= 80;
        uint32_t rec_ptr = (uint32_t)rsp;
        
        uint32_t exception_code = 0xC0000005; // STATUS_ACCESS_VIOLATION
        if (sig == SIGILL) exception_code = 0xC000001D; // STATUS_ILLEGAL_INSTRUCTION
        
        macwi_emu_write_memory(g_current_emu_ctx, rec_ptr, &exception_code, 4);
        
        uint32_t flags = 0;
        macwi_emu_write_memory(g_current_emu_ctx, rec_ptr + 4, &flags, 4);
        
        uint32_t next_record = 0;
        macwi_emu_write_memory(g_current_emu_ctx, rec_ptr + 8, &next_record, 4);
        
        uint32_t ex_fault_addr = (uint32_t)fault_addr; // Store the actual faulting address
        macwi_emu_write_memory(g_current_emu_ctx, rec_ptr + 12, &ex_fault_addr, 4);
        
        uint32_t num_params = 0;
        macwi_emu_write_memory(g_current_emu_ctx, rec_ptr + 16, &num_params, 4);
        
        // Push arguments for KiUserExceptionDispatcher(PEXCEPTION_RECORD, PCONTEXT)
        rsp -= 4;
        uint32_t context_ptr = 0; // Null CONTEXT for now
        macwi_emu_write_memory(g_current_emu_ctx, rsp, &context_ptr, 4);
        
        rsp -= 4;
        macwi_emu_write_memory(g_current_emu_ctx, rsp, &rec_ptr, 4);
        
        rsp -= 4;
        uint32_t dummy_ret = 0;
        macwi_emu_write_memory(g_current_emu_ctx, rsp, &dummy_ret, 4);
        
        macwi_emu_set_sp(g_current_emu_ctx, rsp);
        
        uint64_t ki_user_disp = macwi_thunk_get_trampoline(g_current_emu_ctx, "ntdll.dll", "KiUserExceptionDispatcher");
        macwi_emu_set_pc(g_current_emu_ctx, ki_user_disp);
        
        uint64_t disp_loop = macwi_emu_get_dispatcher_loop(g_current_emu_ctx);
        
        printf("[macwi] Redirecting Guest PC to KiUserExceptionDispatcher (0x%llX)...\n", ki_user_disp);
        
        // Prevent infinite loop if KiUserExceptionDispatcher faults
        static int crash_count = 0;
        if (crash_count++ > 0) {
            printf("[macwi] FATAL: Nested crash detected in exception handler! Exiting.\n");
            exit(1);
        }
        
        if (disp_loop) {
#if defined(__aarch64__) || defined(_M_ARM_64)
            ucontext_t* uc = (ucontext_t*)ucontext;
#ifdef __APPLE__
            uc->uc_mcontext->__ss.__pc = disp_loop;
#else
            uc->uc_mcontext.pc = disp_loop;
#endif
#endif
            return; // Return from signal handler to resume at Dispatcher Loop
        }
    } else {
        fprintf(stderr, "[macwi] Emulator context not available.\n");
    }
    
#if defined(__aarch64__) || defined(_M_ARM_64)
    ucontext_t* uc = (ucontext_t*)ucontext;
#ifdef __APPLE__
    uintptr_t pc = uc->uc_mcontext->__ss.__pc;
#else
    uintptr_t pc = uc->uc_mcontext.pc;
#endif
    fprintf(stderr, "[macwi] Host PC at time of crash: 0x%016llX\n", (uint64_t)pc);
#endif

    fprintf(stderr, "=========================================\n\n");
    _exit(1);
}

static void setup_signals() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = host_signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGTRAP, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
}

static void* emu_thread_func(void* arg) {
    EmuThreadArgs* args = (EmuThreadArgs*)arg;
    g_current_emu_ctx = args->ctx;
    
    printf("\n>>> Starting execution at RIP = 0x%016llX <<<\n\n", args->entry_point);
    fflush(stdout);
    macwi_emu_set_pc(args->ctx, args->entry_point);
    macwi_emu_set_sp(args->ctx, args->rsp);

    macwi_status_t status = macwi_emu_start(args->ctx);
    
    if (status == MACWI_SUCCESS) {
        printf("\n>>> Execution completed gracefully. <<<\n");
    } else {
        printf("\n>>> Execution stopped (status=%d) <<<\n", status);
    }
    fflush(stdout);
    
    // Cleanup and exit the whole process when emulation finishes
    macwi_emu_free(args->ctx);
    printf("emu_thread_func exiting with 0\n");
    fflush(stdout);
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

    setup_signals();

    const char* exe_path = argv[1];
    PE_IMAGE image;

    // 1. Initialize Subsystems
    printf("[macwi] Initializing Virtual File System...\n");
    macwi_vfs_init();
    
    printf("[macwi] Initializing Registry Virtualization...\n");
    macwi_registry_init();
    
    printf("[macwi] Initializing Handle Table...\n");
    macwi_handle_table_init(&g_macwi_handle_table);
    
    extern void macwi_timer_init(void);
    macwi_timer_init();
    
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

    printf("[macwi] Initializing Windows TEB and PEB...\n");
    if (macwi_emu_init_windows_env(ctx, image.image_base, argc, argv) != MACWI_SUCCESS) {
        fprintf(stderr, "[macwi] Failed to initialize Windows Environment\n");
        return EXIT_FAILURE;
    }

    // Register Win32 API Thunks
    macwi_kernel32_register_apis();
    extern void macwi_user32_register_apis(void);
    macwi_user32_register_apis();
    extern void macwi_gdi32_register_apis(void);
    macwi_gdi32_register_apis();
    macwi_ntdll_register_apis();
    macwi_advapi32_register_apis();
    extern void macwi_d3d9_register_apis(void);
    macwi_d3d9_register_apis();
    macwi_thunk_init_dispatcher(ctx);
    macwi_thunk_init_callbacks(ctx);
    
    extern void macwi_d3d9_init_trampolines(EMU_CONTEXT* ctx);
    macwi_d3d9_init_trampolines(ctx);
    
    // Pre-generate KiUserExceptionDispatcher trampoline so we don't allocate memory in a signal handler!
    macwi_thunk_get_trampoline(ctx, "ntdll.dll", "KiUserExceptionDispatcher");

    // 6. Map PE into FEXCore Memory
    printf("[macwi] Mapping PE into Emulator Memory...\n");
    // Apple Silicon uses 16KB (0x4000) page size. Align the allocation size up to 16KB.
    uint64_t map_size = (image.size_of_image + 0x3FFF) & ~0x3FFFULL;
    if (macwi_emu_map_memory(ctx, image.image_base, map_size, MACWI_PROT_ALL, NULL) != MACWI_SUCCESS) {
        fprintf(stderr, "[macwi] Failed to map PE base memory\n");
        return EXIT_FAILURE;
    }
    
    printf("[macwi] Resolving Imports (IAT Patching)...\n");
    if (macwi_pe_resolve_imports(&image, ctx) != MACWI_SUCCESS) {
        fprintf(stderr, "[macwi] Warning: Failed to resolve all imports\n");
    }

    if (macwi_emu_write_memory(ctx, image.image_base, image.mapped_base, image.size_of_image) != MACWI_SUCCESS) {
        fprintf(stderr, "[macwi] Failed to write PE sections to Emulator Memory\n");
        return EXIT_FAILURE;
    }

    // 7. Allocate Guest Stack (2MB)
    uint64_t stack_base = 0;
    uint32_t stack_size = 2 * 1024 * 1024;
    printf("[macwi] Allocating Guest Stack (2MB)...\n");
    if (macwi_emu_map_memory(ctx, 0, stack_size, MACWI_PROT_READ | MACWI_PROT_WRITE, &stack_base) != MACWI_SUCCESS) {
        fprintf(stderr, "[macwi] Failed to map Guest Stack\n");
        return EXIT_FAILURE;
    }
    printf("[macwi] Guest Stack allocated at 0x%016llX\n", stack_base);
    uint64_t rsp_top = stack_base + stack_size - 0x1000; // Leave some space at the top

    // 8. Launch execution thread
    EmuThreadArgs* args = malloc(sizeof(EmuThreadArgs));
    args->ctx = ctx;
    args->entry_point = image.entry_point;
    args->rsp = rsp_top;
    args->image = image;

    // Initialize Cocoa before launching the emulator thread
    extern void macwi_cocoa_init(void);
    macwi_cocoa_init();

    pthread_t thread;
    pthread_create(&thread, NULL, emu_thread_func, args);

    // Provide a simple loop to keep the main thread alive and optionally process GUI
    printf("[macwi] Emulator thread detached. Main thread running Cocoa runloop...\n");
        macwi_cocoa_run_loop();
        return 0;
}
