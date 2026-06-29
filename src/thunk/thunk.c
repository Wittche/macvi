/**
 * @file thunk.c
 * @brief Thunking layer — Parameter marshaling helpers for x86_64.
 *
 * Implements Microsoft x64 Calling Convention parameter extraction.
 * RCX, RDX, R8, R9, then stack.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/thunk.h"
#include "macwi/emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

macwi_status_t macwi_thunk_read_param_32(EMU_CONTEXT* ctx, int param_index, uint32_t* out_val) {
    if (!ctx || !out_val || param_index < 0) return MACWI_ERROR_INVALID_PARAM;
    // 32-bit stdcall/cdecl: arguments are on the stack.
    // [ESP] = return address
    // [ESP+4] = arg 0, [ESP+8] = arg 1, etc.
    uint64_t esp = macwi_emu_get_sp(ctx);
    uint64_t param_addr = esp + 4 + (param_index * 4);
    return macwi_emu_read_memory(ctx, param_addr, out_val, 4);
}

macwi_status_t macwi_thunk_read_param_64(EMU_CONTEXT* ctx, int param_index, uint64_t* out_val) {
    if (!ctx || !out_val || param_index < 0) return MACWI_ERROR_INVALID_PARAM;
    // In 32-bit mode, 64-bit parameters are often passed as two 32-bit parameters.
    // For now, since most of our emulation assumes pointers are passed, we just read one 32-bit value and zero-extend it.
    uint32_t val32;
    macwi_status_t st = macwi_thunk_read_param_32(ctx, param_index, &val32);
    if (st == MACWI_SUCCESS) {
        *out_val = (uint64_t)val32;
    }
    return st;
}

macwi_status_t macwi_thunk_read_guest_string(EMU_CONTEXT* ctx, uint64_t guest_addr, char* out_buf, size_t max_len) {
    if (!ctx || !out_buf || max_len == 0) return MACWI_ERROR_INVALID_PARAM;
    if (guest_addr == 0) {
        out_buf[0] = '\0';
        return MACWI_SUCCESS;
    }
    
    for (size_t i = 0; i < max_len - 1; i++) {
        uint8_t c;
        if (macwi_emu_read_memory(ctx, guest_addr + i, &c, 1) != MACWI_SUCCESS) {
            out_buf[i] = '\0';
            return MACWI_ERROR_MEMORY;
        }
        out_buf[i] = c;
        if (c == '\0') return MACWI_SUCCESS;
    }
    
    out_buf[max_len - 1] = '\0';
    return MACWI_SUCCESS;
}

macwi_status_t macwi_thunk_string_out(EMU_CONTEXT* ctx, uint64_t guest_addr, const char* in_str, size_t max_len) {
    if (!ctx || !in_str || max_len == 0) return MACWI_ERROR_INVALID_PARAM;
    
    size_t len = strlen(in_str);
    if (len >= max_len) {
        len = max_len - 1;
    }
    
    if (macwi_emu_write_memory(ctx, guest_addr, in_str, len) != MACWI_SUCCESS) {
        return MACWI_ERROR_MEMORY;
    }
    
    uint8_t zero = 0;
    if (macwi_emu_write_memory(ctx, guest_addr + len, &zero, 1) != MACWI_SUCCESS) {
        return MACWI_ERROR_MEMORY;
    }
    
    return MACWI_SUCCESS;
}

macwi_status_t macwi_thunk_stdcall_return(EMU_CONTEXT* ctx, int param_count) {
    // In FEXCore, the trampoline's "ret" instruction handles the return!
    // We do NOT need to manually modify RIP/ESP here because we return to the trampoline!
    // We just return success and let the trampoline handle it.
    return MACWI_SUCCESS;
}

/* ============================================================================
 * API Registration & Dispatcher
 * ============================================================================ */

#define MAX_APIS 1024
typedef struct {
    char dll_name[64];
    char func_name[64];
    macwi_win32_api_cb callback;
    int param_count;
    uint64_t trampoline_va;
} REGISTERED_API;

static REGISTERED_API g_apis[MAX_APIS];
static int g_api_count = 0;

static uint64_t g_trampoline_base = 0;
static int g_trampoline_offset = 0;

macwi_status_t macwi_thunk_register_api(const char* dll_name, const char* func_name, macwi_win32_api_cb callback, int param_count) {
    if (g_api_count >= MAX_APIS) return MACWI_ERROR_MEMORY;
    REGISTERED_API* api = &g_apis[g_api_count++];
    strncpy(api->dll_name, dll_name, sizeof(api->dll_name)-1);
    strncpy(api->func_name, func_name, sizeof(api->func_name)-1);
    api->callback = callback;
    api->param_count = param_count;
    api->trampoline_va = 0;
    return MACWI_SUCCESS;
}

uint64_t macwi_thunk_get_magic_va(const char* dll_name, const char* func_name) {
    // Deprecated: use macwi_thunk_get_trampoline instead
    return 0;
}

uint64_t macwi_thunk_get_trampoline(EMU_CONTEXT* ctx, const char* dll_name, const char* func_name) {
    if (g_trampoline_base == 0) {
        if (macwi_emu_map_memory(ctx, 0, 0x10000, MACWI_PROT_READ | MACWI_PROT_WRITE | MACWI_PROT_EXEC, &g_trampoline_base) != MACWI_SUCCESS) {
            fprintf(stderr, "[macwi:thunk] Failed to allocate trampoline page\n");
            return 0;
        }
    }

    for (int i = 0; i < g_api_count; i++) {
        if (strcasecmp(g_apis[i].dll_name, dll_name) == 0 &&
            strcasecmp(g_apis[i].func_name, func_name) == 0) {
            
            if (g_apis[i].trampoline_va != 0) return g_apis[i].trampoline_va;

            // Generate trampoline
            // B8 <api_index>      ; mov eax, api_index
            // CD 80               ; int 0x80
            // C2 <params*4>       ; ret imm16 (stdcall) or C3 (ret for cdecl)

            uint8_t tramp[16];
            int t_len = 0;
            
            tramp[t_len++] = 0xB8; // mov eax
            *(uint32_t*)(&tramp[t_len]) = (uint32_t)i;
            t_len += 4;
            
            tramp[t_len++] = 0xCD; // int
            tramp[t_len++] = 0x80; // 0x80
            
            if (g_apis[i].param_count > 0) {
                tramp[t_len++] = 0xC2; // ret imm16
                uint16_t pop_bytes = (uint16_t)(g_apis[i].param_count * 4);
                *(uint16_t*)(&tramp[t_len]) = pop_bytes;
                t_len += 2;
            } else {
                tramp[t_len++] = 0xC3; // ret
            }

            uint64_t addr = g_trampoline_base + g_trampoline_offset;
            macwi_emu_write_memory(ctx, addr, tramp, t_len);
            
            g_apis[i].trampoline_va = addr;
            g_trampoline_offset += 16;
            
            return addr;
        }
    }
    return 0;
}

macwi_status_t macwi_thunk_handle_syscall(EMU_CONTEXT* ctx, uint32_t api_index) {
    if (api_index >= g_api_count) return MACWI_ERROR_INVALID_PARAM;
    g_apis[api_index].callback(ctx);
    return MACWI_SUCCESS;
}

macwi_status_t macwi_thunk_init_dispatcher(EMU_CONTEXT* ctx) {
    // Handled by FEXCore SyscallHandler natively. No action required here.
    return MACWI_SUCCESS;
}

/* ============================================================================
 * Callback Infrastructure
 * ============================================================================ */

#define MAX_CALLBACK_DEPTH 16
static __thread MACWI_CALLBACK_STATE g_callback_stack[MAX_CALLBACK_DEPTH];
static __thread int g_callback_depth = 0;
static uint64_t g_callback_trampoline_va = 0;

static void internal_CallbackReturn(EMU_CONTEXT* ctx) {
    if (g_callback_depth <= 0) {
        fprintf(stderr, "[macwi:thunk] FATAL: CallbackReturn without active callback\n");
        return;
    }
    g_callback_depth--;
    MACWI_CALLBACK_STATE* state = &g_callback_stack[g_callback_depth];
    
    // EAX contains the return value from the guest WindowProc
    uint32_t ret_val = macwi_emu_reg_read_32(ctx, 0);
    
    // Restore CPU state to exactly before the syscall that triggered the callback
    macwi_emu_restore_state(ctx, state->cpu_state);
    macwi_emu_free_state(state->cpu_state);

    // Simulate the return value of the API that triggered the callback (e.g., DispatchMessage)
    macwi_emu_reg_write_32(ctx, 0, ret_val);
    
    // WORKAROUND FOR FEXCore:
    // FEXCore's int 0x80 implementation unconditionally sets RIP = PC + 2 (the instruction after int 0x80).
    // Because we are currently inside an int 0x80 triggered by the CallbackReturn trampoline, 
    // FEXCore will overwrite our restored RIP and jump to the instruction AFTER CallbackReturn's int 0x80.
    // The instruction after CallbackReturn's int 0x80 is 'ret'.
    // To make this 'ret' jump to the CORRECT original trampoline (where we actually want to resume execution),
    // we must push the desired target RIP (original trampoline's int 0x80 + 2) onto the guest stack.
    
    uint32_t target_rip = (uint32_t)macwi_emu_get_pc(ctx) + 2; // Original trampoline's int 0x80 + 2 (which is 'ret' or 'ret N')
    uint32_t guest_sp = (uint32_t)macwi_emu_get_sp(ctx);
    guest_sp -= 4; // Push 32-bit address
    
    macwi_emu_write_memory(ctx, guest_sp, &target_rip, 4);
    macwi_emu_set_sp(ctx, guest_sp);
}

macwi_status_t macwi_thunk_init_callbacks(EMU_CONTEXT* ctx) {
    if (g_callback_trampoline_va != 0) return MACWI_SUCCESS; // Already initialized
    macwi_thunk_register_api("macwi_internal", "CallbackReturn", internal_CallbackReturn, 0);
    g_callback_trampoline_va = macwi_thunk_get_trampoline(ctx, "macwi_internal", "CallbackReturn");
    return MACWI_SUCCESS;
}

macwi_status_t macwi_thunk_invoke_callback(EMU_CONTEXT* ctx, uint32_t target_addr, uint32_t arg_count, const uint32_t* args, uint32_t caller_pop_bytes, MACWI_CALLBACK_STATE* out_state) {
    if (g_callback_depth >= MAX_CALLBACK_DEPTH) return MACWI_ERROR_MEMORY;
    if (g_callback_trampoline_va == 0) return MACWI_ERROR_INVALID_PARAM;
    
    MACWI_CALLBACK_STATE* state = &g_callback_stack[g_callback_depth++];
    macwi_emu_save_state(ctx, &state->cpu_state);
    if (out_state) *out_state = *state;
    
    uint64_t sp = macwi_emu_get_sp(ctx);
    uint64_t orig_sp = sp;
    
    // Build the callback stack frame for the thunk stub's `ret <caller_pop_bytes>`.
    //
    // After HandleSyscall returns, FEXCore restores all guest registers
    // (including ESP) from CpuStateFrame via FillStaticRegs, then executes
    // the next x86 instruction: `ret <caller_pop_bytes>`.
    //
    // x86 `ret N` does:
    //   1. EIP = pop dword [ESP]    (ESP += 4)
    //   2. ESP += N                  (stdcall param cleanup)
    //
    // We set up the stack so ret N:
    //   - Pops target_addr into EIP (jumps to callback)
    //   - Skips caller_pop_bytes of zero-initialized padding
    //   - Leaves ESP pointing at the callback's frame:
    //     [ESP] = trampoline return addr
    //     [ESP+4] = arg0, [ESP+8] = arg1, ...
    
    // Push callback arguments right-to-left (stdcall convention)
    for (int i = arg_count - 1; i >= 0; i--) {
        sp -= 4;
        macwi_emu_write_memory(ctx, sp, &args[i], 4);
    }
    
    // Push CallbackReturn trampoline as the callback's return address
    sp -= 4;
    uint32_t ret_addr = (uint32_t)g_callback_trampoline_va;
    macwi_emu_write_memory(ctx, sp, &ret_addr, 4);
    
    // Zero-initialize padding for the thunk's `ret N` ESP adjustment.
    // Previously this was uninitialised, which could cause JIT to read
    // garbage values and crash.
    uint32_t zero = 0;
    for (uint32_t i = 0; i < caller_pop_bytes; i += 4) {
        sp -= 4;
        macwi_emu_write_memory(ctx, sp, &zero, 4);
    }
    
    // Push target_addr for the `ret N` EIP pop
    sp -= 4;
    macwi_emu_write_memory(ctx, sp, &target_addr, 4);
    
    macwi_emu_set_sp(ctx, sp);
    
    printf("[macwi:thunk] invoke_callback: target=%x, orig_sp=%llx, new_sp=%llx, ret=%x, pop=%u\n",
           target_addr, orig_sp, sp, ret_addr, caller_pop_bytes);
    fflush(stdout);
    
    return MACWI_SUCCESS;
}

