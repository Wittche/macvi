/**
 * @file ntdll.c
 * @brief Win32 ntdll.dll stub implementations for PE32+ (64-bit).
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/types.h"
#include "macwi/emu.h"
#include "macwi/thunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NT_STUB_LOG(fmt, ...) fprintf(stderr, "[macwi:ntdll] " fmt "\n", ##__VA_ARGS__)

#define STATUS_SUCCESS               0x00000000
#define STATUS_INVALID_PARAMETER     0xC000000D
#define STATUS_NOT_IMPLEMENTED       0xC0000002

static void ntdll_NtCreateFile(EMU_CONTEXT* ctx) {
    NT_STUB_LOG("NtCreateFile stubbed");
    macwi_emu_reg_write_64(ctx, 0, STATUS_NOT_IMPLEMENTED);
    macwi_thunk_stdcall_return(ctx, 11);
}

static void ntdll_NtClose(EMU_CONTEXT* ctx) {
    uint64_t handle;
    macwi_thunk_read_param_64(ctx, 0, &handle);
    NT_STUB_LOG("NtClose(0x%llX) stubbed", handle);
    macwi_emu_reg_write_64(ctx, 0, STATUS_SUCCESS);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void ntdll_NtReadFile(EMU_CONTEXT* ctx) {
    NT_STUB_LOG("NtReadFile stubbed");
    macwi_emu_reg_write_64(ctx, 0, STATUS_NOT_IMPLEMENTED);
    macwi_thunk_stdcall_return(ctx, 9);
}

static void ntdll_NtWriteFile(EMU_CONTEXT* ctx) {
    NT_STUB_LOG("NtWriteFile stubbed");
    macwi_emu_reg_write_64(ctx, 0, STATUS_NOT_IMPLEMENTED);
    macwi_thunk_stdcall_return(ctx, 9);
}

static void ntdll_NtAllocateVirtualMemory(EMU_CONTEXT* ctx) {
    NT_STUB_LOG("NtAllocateVirtualMemory stubbed");
    macwi_emu_reg_write_64(ctx, 0, STATUS_NOT_IMPLEMENTED);
    macwi_thunk_stdcall_return(ctx, 6);
}

static void ntdll_NtFreeVirtualMemory(EMU_CONTEXT* ctx) {
    NT_STUB_LOG("NtFreeVirtualMemory stubbed");
    macwi_emu_reg_write_64(ctx, 0, STATUS_NOT_IMPLEMENTED);
    macwi_thunk_stdcall_return(ctx, 4);
}

static void ntdll_RtlInitUnicodeString(EMU_CONTEXT* ctx) {
    NT_STUB_LOG("RtlInitUnicodeString stubbed");
    macwi_emu_reg_write_64(ctx, 0, 0); // void function usually
    macwi_thunk_stdcall_return(ctx, 2);
}

static void ntdll_RtlFreeUnicodeString(EMU_CONTEXT* ctx) {
    NT_STUB_LOG("RtlFreeUnicodeString stubbed");
    macwi_emu_reg_write_64(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void ntdll_KiUserExceptionDispatcher(EMU_CONTEXT* ctx) {
    NT_STUB_LOG("KiUserExceptionDispatcher called!");
    // In a real OS, this would call RtlDispatchException.
    // For now, we just print the exception record and terminate.
    
    // The exception record and context are passed on the stack in x86/x64,
    // or we can just extract them. Since we are stubbing it, we can just print and exit.
    uint64_t exception_record_ptr;
    // Assuming we pushed it to the stack or passed it in RCX (param 0)
    macwi_thunk_read_param_64(ctx, 0, &exception_record_ptr);
    
    if (exception_record_ptr) {
        uint32_t exception_code;
        macwi_emu_read_memory(ctx, exception_record_ptr, &exception_code, 4);
        NT_STUB_LOG("Unhandled Exception Code: 0x%08X", exception_code);
    }
    
    // Terminate Process
    exit(1337);
}

/* ============================================================================
 * API Registration
 * ============================================================================ */

void macwi_ntdll_register_apis(void) {
    macwi_thunk_register_api("ntdll.dll", "NtCreateFile", ntdll_NtCreateFile, 11);
    macwi_thunk_register_api("ntdll.dll", "NtClose", ntdll_NtClose, 1);
    macwi_thunk_register_api("ntdll.dll", "NtReadFile", ntdll_NtReadFile, 9);
    macwi_thunk_register_api("ntdll.dll", "NtWriteFile", ntdll_NtWriteFile, 9);
    macwi_thunk_register_api("ntdll.dll", "NtAllocateVirtualMemory", ntdll_NtAllocateVirtualMemory, 6);
    macwi_thunk_register_api("ntdll.dll", "NtFreeVirtualMemory", ntdll_NtFreeVirtualMemory, 4);
    macwi_thunk_register_api("ntdll.dll", "RtlInitUnicodeString", ntdll_RtlInitUnicodeString, 2);
    macwi_thunk_register_api("ntdll.dll", "RtlFreeUnicodeString", ntdll_RtlFreeUnicodeString, 1);
    macwi_thunk_register_api("ntdll.dll", "KiUserExceptionDispatcher", ntdll_KiUserExceptionDispatcher, 2);
}
