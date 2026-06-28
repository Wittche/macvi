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
    // Left empty for now, as we only need 32-bit in Phase 8
    return MACWI_ERROR_UNSUPPORTED;
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
