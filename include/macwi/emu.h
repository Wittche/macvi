/**
 * @file emu.h
 * @brief x86 Emulation engine abstraction layer.
 *
 * This layer wraps the Unicorn Engine (or another backend) to provide a
 * standard interface for the rest of MacWI to map memory, write memory,
 * and execute x86 instructions.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "macwi/types.h"
#include <stddef.h>
#include <stdint.h>

/** Opaque handle for an emulation context */
typedef struct EMU_CONTEXT EMU_CONTEXT;

/** Memory protection flags (consistent with Unicorn's UC_PROT_*) */
#define MACWI_PROT_NONE  0
#define MACWI_PROT_READ  1
#define MACWI_PROT_WRITE 2
#define MACWI_PROT_EXEC  4
#define MACWI_PROT_ALL   7

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the emulation context for an x86 32-bit machine.
 *
 * @param out_ctx Pointer to receive the allocated context.
 * @return MACWI_SUCCESS on success, error code otherwise.
 */
macwi_status_t macwi_emu_init(EMU_CONTEXT** out_ctx);

/**
 * Free the emulation context and release resources.
 *
 * @param ctx Context to free.
 */
void macwi_emu_free(EMU_CONTEXT* ctx);

/**
 * Map a region of memory in the emulator's isolated address space.
 * 
 * Note: `address` and `size` must be page-aligned (typically 4KB for x86).
 *
 * @param ctx Context handle.
 * @param address The virtual address in the guest to map.
 * @param size The size of the mapping.
 * @param perms Protection flags (MACWI_PROT_*).
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_emu_map_memory(EMU_CONTEXT* ctx, uint32_t address, size_t size, uint32_t perms);

/**
 * Unmap a region of memory from the emulator.
 *
 * @param ctx Context handle.
 * @param address The starting virtual address.
 * @param size The size of the region in bytes.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_emu_unmap_memory(EMU_CONTEXT* ctx, uint32_t address, size_t size);

/**
 * Write data to the emulator's memory.
 *
 * @param ctx Context handle.
 * @param address The target virtual address.
 * @param data The data to write.
 * @param size The number of bytes to write.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_emu_write_memory(EMU_CONTEXT* ctx, uint32_t address, const void* data, size_t size);

/**
 * Read data from the emulator's memory.
 *
 * @param ctx Context handle.
 * @param address The source virtual address.
 * @param data The buffer to receive data.
 * @param size The number of bytes to read.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_emu_read_memory(EMU_CONTEXT* ctx, uint32_t address, void* data, size_t size);

/**
 * Read the value of a specific x86 register.
 *
 * @param ctx Context handle.
 * @param reg_id A predefined register ID (0=EAX, 1=EBX, 2=ECX, 3=EDX, 4=ESI, 5=EDI, 6=EBP, 7=ESP, 8=EIP)
 * @param out_value Pointer to receive the register value.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_emu_reg_read(EMU_CONTEXT* ctx, int reg_id, uint32_t* out_value);

/**
 * Write a value to a specific x86 register.
 *
 * @param ctx Context handle.
 * @param reg_id A predefined register ID.
 * @param value The value to write.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_emu_reg_write(EMU_CONTEXT* ctx, int reg_id, uint32_t value);

/**
 * Start execution of the emulator.
 *
 * @param ctx Context handle.
 * @param entry_point The guest address to start execution from.
 * @param stack_top The guest address of the top of the stack.
 * @return MACWI_SUCCESS on normal exit, error on crash/exception.
 */
macwi_status_t macwi_emu_start(EMU_CONTEXT* ctx, uint32_t entry_point, uint32_t stack_top);

/**
 * Callback signature for code execution hooks.
 *
 * @param ctx The emulation context.
 * @param address The address being executed.
 * @param size The size of the instruction.
 * @param user_data Arbitrary user data passed during registration.
 */
typedef void (*macwi_emu_hook_cb)(EMU_CONTEXT* ctx, uint32_t address, uint32_t size, void* user_data);

/**
 * Add a hook to intercept code execution in a specific address range.
 *
 * @param ctx Context handle.
 * @param begin Starting address.
 * @param end Ending address.
 * @param callback The function to call.
 * @param user_data Opaque pointer passed to the callback.
 * @param out_hook_handle Pointer to receive an opaque handle (can be NULL).
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_emu_add_code_hook(EMU_CONTEXT* ctx, uint32_t begin, uint32_t end, macwi_emu_hook_cb callback, void* user_data, void** out_hook_handle);

#ifdef __cplusplus
}
#endif
