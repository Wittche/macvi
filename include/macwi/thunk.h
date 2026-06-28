/**
 * @file thunk.h
 * @brief Thunking layer API — Syscall dispatcher and parameter marshaling.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "macwi/types.h"
#include "macwi/emu.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * API Registration & Dispatcher
 * ============================================================================ */

/** Function signature for a native Win32 stub */
typedef void (*macwi_win32_api_cb)(EMU_CONTEXT* ctx);

/**
 * Register a native C function to handle a specific Win32 API.
 *
 * @param dll_name The name of the DLL (e.g., "kernel32.dll").
 * @param func_name The name of the function (e.g., "GetTickCount").
 * @param callback The native C function to execute.
 * @param param_count The number of parameters this function takes.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_thunk_register_api(const char* dll_name, const char* func_name, macwi_win32_api_cb callback, int param_count);

/**
 * Find the Magic VA (Virtual Address) allocated for a registered API.
 * DEPRECATED: use macwi_thunk_get_trampoline instead.
 *
 * @param dll_name DLL name.
 * @param func_name Function name.
 * @return The 64-bit magic address, or 0 if not found.
 */
uint64_t macwi_thunk_get_magic_va(const char* dll_name, const char* func_name);

/**
 * Get or generate the guest virtual address of the trampoline for a registered API.
 *
 * @param ctx Emulation context (used to allocate the trampoline page if needed).
 * @param dll_name DLL name.
 * @param func_name Function name.
 * @return The 64-bit guest address of the trampoline, or 0 if not found.
 */
uint64_t macwi_thunk_get_trampoline(EMU_CONTEXT* ctx, const char* dll_name, const char* func_name);

/**
 * Handle a syscall from the guest (e.g., int 0x80) used for API dispatch.
 * 
 * @param ctx Emulation context.
 * @param api_index The index of the API (passed in EAX).
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_thunk_handle_syscall(EMU_CONTEXT* ctx, uint32_t api_index);
macwi_status_t macwi_thunk_init_dispatcher(EMU_CONTEXT* ctx);

/**
 * @brief Represents a saved callback state.
 */
typedef struct {
    void* cpu_state;
} MACWI_CALLBACK_STATE;

/**
 * @brief Invoke a guest callback function from a host syscall handler.
 */
macwi_status_t macwi_thunk_invoke_callback(EMU_CONTEXT* ctx, uint32_t target_addr, uint32_t arg_count, const uint32_t* args, MACWI_CALLBACK_STATE* out_state);

/**
 * @brief Initialize callback infrastructure.
 */
macwi_status_t macwi_thunk_init_callbacks(EMU_CONTEXT* ctx);

/* ============================================================================
 * Parameter Marshaling Helpers
 * ============================================================================ */

/**
 * Read a 32-bit parameter.
 */
macwi_status_t macwi_thunk_read_param_32(EMU_CONTEXT* ctx, int param_index, uint32_t* out_val);

/**
 * Read a 64-bit parameter (x64 calling convention: RCX, RDX, R8, R9, Stack).
 */
macwi_status_t macwi_thunk_read_param_64(EMU_CONTEXT* ctx, int param_index, uint64_t* out_val);

/**
 * Read a null-terminated string from guest memory into a host buffer.
 *
 * @param ctx Emulation context.
 * @param guest_addr The 64-bit guest pointer.
 * @param out_buf Host buffer to write to.
 * @param max_len Maximum bytes to read.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_thunk_read_guest_string(EMU_CONTEXT* ctx, uint64_t guest_addr, char* out_buf, size_t max_len);

/**
 * Write a null-terminated string from a host buffer to guest memory.
 *
 * @param ctx Emulation context.
 * @param guest_addr The 64-bit guest pointer.
 * @param in_str Host string to read from.
 * @param max_len Maximum bytes to write (including null terminator).
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_thunk_string_out(EMU_CONTEXT* ctx, uint64_t guest_addr, const char* in_str, size_t max_len);

/**
 * Clean up the guest stack after an API returns.
 * In x64, caller cleans up, so we just pop return address and jump.
 *
 * @param ctx Emulation context.
 * @param param_count Number of parameters.
 * @return MACWI_SUCCESS.
 */
macwi_status_t macwi_thunk_stdcall_return(EMU_CONTEXT* ctx, int param_count);

#ifdef __cplusplus
}
#endif
