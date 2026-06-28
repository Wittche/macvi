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

/** Memory protection flags */
#define MACWI_PROT_NONE  0
#define MACWI_PROT_READ  1
#define MACWI_PROT_WRITE 2
#define MACWI_PROT_EXEC  4
#define MACWI_PROT_ALL   7

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the emulation context for an x86_64 64-bit machine.
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
 * @brief Map memory in the guest address space.
 */
macwi_status_t macwi_emu_map_memory(EMU_CONTEXT* ctx, uint64_t address, size_t size, int perms, uint64_t* out_mapped_address);

/**
 * @brief Unmap memory from the guest address space.
 */
macwi_status_t macwi_emu_unmap_memory(EMU_CONTEXT* ctx, uint64_t address, size_t size);

/**
 * Write data into the emulator's memory.
 *
 * @param ctx Context handle.
 * @param address The virtual address to write to.
 * @param data Pointer to the data to write.
 * @param size Size of the data in bytes.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_emu_write_memory(EMU_CONTEXT* ctx, uint64_t address, const void* data, size_t size);

macwi_status_t macwi_emu_init_windows_env(EMU_CONTEXT* ctx, uint64_t image_base, int argc, char** argv);

/**
 * @brief Create a new emulated thread
 */
macwi_status_t macwi_emu_create_thread(EMU_CONTEXT* ctx, uint64_t entry_point, uint64_t param, uint64_t stack_size, uint64_t* out_thread_id);

/**
 * Read data from the emulator's memory.
 *
 * @param ctx Context handle.
 * @param address The virtual address to read from.
 * @param out_data Pointer to the buffer to receive the data.
 * @param size Size of the data to read in bytes.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_emu_read_memory(EMU_CONTEXT* ctx, uint64_t address, void* out_data, size_t size);

/**
 * Read a 32-bit register.
 *
 * @param ctx Context handle.
 * @param reg_id Register ID (0=eax, 1=ecx, 2=edx, 3=ebx, 4=esp, 5=ebp, 6=esi, 7=edi).
 * @return The value of the register.
 */
uint32_t macwi_emu_reg_read_32(EMU_CONTEXT* ctx, int reg_id);

/**
 * Write a 32-bit register.
 *
 * @param ctx Context handle.
 * @param reg_id Register ID.
 * @param value The value to write.
 */
void macwi_emu_reg_write_32(EMU_CONTEXT* ctx, int reg_id, uint32_t value);

/**
 * Read a 64-bit register.
 *
 * @param ctx Context handle.
 * @param reg_id Register ID (0=rax, 1=rcx, 2=rdx, 3=rbx, 4=rsp, 5=rbp, 6=rsi, 7=rdi, 8=r8...).
 * @return The value of the register.
 */
uint64_t macwi_emu_reg_read_64(EMU_CONTEXT* ctx, int reg_id);

/**
 * Write a 64-bit register.
 *
 * @param ctx Context handle.
 * @param reg_id Register ID.
 * @param value The value to write.
 */
void macwi_emu_reg_write_64(EMU_CONTEXT* ctx, int reg_id, uint64_t value);

/**
 * Set the Program Counter (RIP).
 */
void macwi_emu_set_pc(EMU_CONTEXT* ctx, uint64_t pc);

/**
 * Get the Program Counter (RIP).
 */
uint64_t macwi_emu_get_pc(EMU_CONTEXT* ctx);

/**
 * Set the Stack Pointer (RSP).
 */
void macwi_emu_set_sp(EMU_CONTEXT* ctx, uint64_t sp);

/**
 * Get the Stack Pointer (RSP).
 */
uint64_t macwi_emu_get_sp(EMU_CONTEXT* ctx);

/**
 * Start execution from the current PC.
 *
 * @param ctx Context handle.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_emu_start(EMU_CONTEXT* ctx);

/**
 * Stop execution.
 */
void macwi_emu_stop(EMU_CONTEXT* ctx);

/**
 * Callback signature for code execution hooks.
 *
 * @param ctx The emulation context.
 * @param address The address being executed.
 * @param user_data Arbitrary user data passed during registration.
 */
typedef void (*macwi_emu_hook_cb)(EMU_CONTEXT* ctx, uint64_t address, void* user_data);

/**
 * Add a hook to intercept code execution in a specific address range.
 *
 * @param ctx Context handle.
 * @param callback The function to call.
 * @param user_data Opaque pointer passed to the callback.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_emu_hook_unmapped(EMU_CONTEXT* ctx, macwi_emu_hook_cb callback, void* user_data);

#ifdef __cplusplus
}
#endif
