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
 * @param param_count The number of 32-bit parameters this function takes (for stdcall cleanup).
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_thunk_register_api(const char* dll_name, const char* func_name, macwi_win32_api_cb callback, int param_count);

/**
 * Find the Magic VA (Virtual Address) allocated for a registered API.
 *
 * @param dll_name DLL name.
 * @param func_name Function name.
 * @return The 32-bit magic address, or 0 if not found.
 */
uint32_t macwi_thunk_get_magic_va(const char* dll_name, const char* func_name);

/**
 * Initialize the thunking dispatcher and register it with the emulator context.
 * This sets up the Unicorn hook on the magic memory region.
 *
 * @param ctx The emulation context.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_thunk_init_dispatcher(EMU_CONTEXT* ctx);

/* ============================================================================
 * Parameter Marshaling Helpers
 * ============================================================================ */

/**
 * Read a 32-bit parameter from the guest stack (stdcall convention).
 * param_index = 0 is the first parameter (at ESP + 4).
 *
 * @param ctx The emulation context.
 * @param param_index The index of the parameter (0, 1, 2...).
 * @param out_val Pointer to receive the 32-bit value.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_thunk_read_param_32(EMU_CONTEXT* ctx, int param_index, uint32_t* out_val);

/**
 * Read a null-terminated string from guest memory into a host buffer.
 *
 * @param ctx Emulation context.
 * @param guest_addr The 32-bit guest pointer.
 * @param out_buf Host buffer to write to.
 * @param max_len Maximum bytes to read.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_thunk_read_guest_string(EMU_CONTEXT* ctx, uint32_t guest_addr, char* out_buf, size_t max_len);

/**
 * Write a null-terminated string from a host buffer to guest memory.
 *
 * @param ctx Emulation context.
 * @param guest_addr The 32-bit guest pointer.
 * @param in_str Host string to read from.
 * @param max_len Maximum bytes to write (including null terminator).
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_thunk_string_out(EMU_CONTEXT* ctx, uint32_t guest_addr, const char* in_str, size_t max_len);

/**
 * Clean up the guest stack after a stdcall API returns.
 * In stdcall, the callee cleans up the stack: `ret n`.
 *
 * @param ctx Emulation context.
 * @param param_count Number of parameters to pop.
 * @return MACWI_SUCCESS.
 */
macwi_status_t macwi_thunk_stdcall_return(EMU_CONTEXT* ctx, int param_count);

#ifdef __cplusplus
}
#endif
