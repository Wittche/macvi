/**
 * @file emu.c
 * @brief x86 Emulation engine implementation using Unicorn Engine.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/emu.h"
#include <unicorn/unicorn.h>
#include <stdlib.h>
#include <stdio.h>

struct EMU_CONTEXT {
    uc_engine *uc;
};

/* Helper to map generic register IDs to Unicorn IDs */
static int map_reg_id(int reg_id) {
    switch (reg_id) {
        case 0: return UC_X86_REG_EAX;
        case 1: return UC_X86_REG_EBX;
        case 2: return UC_X86_REG_ECX;
        case 3: return UC_X86_REG_EDX;
        case 4: return UC_X86_REG_ESI;
        case 5: return UC_X86_REG_EDI;
        case 6: return UC_X86_REG_EBP;
        case 7: return UC_X86_REG_ESP;
        case 8: return UC_X86_REG_EIP;
        default: return -1;
    }
}

macwi_status_t macwi_emu_init(EMU_CONTEXT** out_ctx) {
    if (!out_ctx) return MACWI_ERROR_INVALID_PARAM;

    EMU_CONTEXT* ctx = (EMU_CONTEXT*)calloc(1, sizeof(EMU_CONTEXT));
    if (!ctx) return MACWI_ERROR_MEMORY;

    /* Initialize Unicorn for x86 32-bit */
    uc_err err = uc_open(UC_ARCH_X86, UC_MODE_32, &ctx->uc);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[macwi_emu] Failed to init Unicorn: %s\n", uc_strerror(err));
        free(ctx);
        return MACWI_ERROR_UNSUPPORTED;
    }

    *out_ctx = ctx;
    return MACWI_SUCCESS;
}

void macwi_emu_free(EMU_CONTEXT* ctx) {
    if (ctx) {
        if (ctx->uc) {
            uc_close(ctx->uc);
        }
        free(ctx);
    }
}

macwi_status_t macwi_emu_map_memory(EMU_CONTEXT* ctx, uint32_t address, size_t size, uint32_t perms) {
    if (!ctx || !ctx->uc) return MACWI_ERROR_INVALID_PARAM;

    /* Align size to 4KB (standard x86 page size) */
    size_t aligned_size = (size + 0xFFF) & ~0xFFF;
    /* Address must also be 4KB aligned */
    if (address & 0xFFF) return MACWI_ERROR_INVALID_PARAM;

    uc_err err = uc_mem_map(ctx->uc, address, aligned_size, perms);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[macwi_emu] Failed to map memory at 0x%08X (size %zu): %s\n", 
                address, aligned_size, uc_strerror(err));
        return MACWI_ERROR_MEMORY;
    }

    return MACWI_SUCCESS;
}

macwi_status_t macwi_emu_unmap_memory(EMU_CONTEXT* ctx, uint32_t address, size_t size) {
    if (!ctx || !ctx->uc || size == 0) return MACWI_ERROR_INVALID_PARAM;

    uc_err err = uc_mem_unmap(ctx->uc, address, size);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[macwi_emu] Failed to unmap memory at 0x%08X (size %zu): %s\n", address, size, uc_strerror(err));
        return MACWI_ERROR_IO;
    }
    return MACWI_SUCCESS;
}

macwi_status_t macwi_emu_write_memory(EMU_CONTEXT* ctx, uint32_t address, const void* data, size_t size) {
    if (!ctx || !ctx->uc || !data) return MACWI_ERROR_INVALID_PARAM;

    uc_err err = uc_mem_write(ctx->uc, address, data, size);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[macwi_emu] Failed to write memory at 0x%08X: %s\n", address, uc_strerror(err));
        return MACWI_ERROR_IO;
    }
    return MACWI_SUCCESS;
}

macwi_status_t macwi_emu_read_memory(EMU_CONTEXT* ctx, uint32_t address, void* data, size_t size) {
    if (!ctx || !ctx->uc || !data) return MACWI_ERROR_INVALID_PARAM;

    uc_err err = uc_mem_read(ctx->uc, address, data, size);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[macwi_emu] Failed to read memory at 0x%08X: %s\n", address, uc_strerror(err));
        return MACWI_ERROR_IO;
    }
    return MACWI_SUCCESS;
}

macwi_status_t macwi_emu_reg_read(EMU_CONTEXT* ctx, int reg_id, uint32_t* out_value) {
    if (!ctx || !ctx->uc || !out_value) return MACWI_ERROR_INVALID_PARAM;

    int uc_reg = map_reg_id(reg_id);
    if (uc_reg < 0) return MACWI_ERROR_INVALID_PARAM;

    uc_err err = uc_reg_read(ctx->uc, uc_reg, out_value);
    if (err != UC_ERR_OK) return MACWI_ERROR_IO;

    return MACWI_SUCCESS;
}

macwi_status_t macwi_emu_reg_write(EMU_CONTEXT* ctx, int reg_id, uint32_t value) {
    if (!ctx || !ctx->uc) return MACWI_ERROR_INVALID_PARAM;

    int uc_reg = map_reg_id(reg_id);
    if (uc_reg < 0) return MACWI_ERROR_INVALID_PARAM;

    uc_err err = uc_reg_write(ctx->uc, uc_reg, &value);
    if (err != UC_ERR_OK) return MACWI_ERROR_IO;

    return MACWI_SUCCESS;
}

macwi_status_t macwi_emu_start(EMU_CONTEXT* ctx, uint32_t entry_point, uint32_t stack_top) {
    if (!ctx || !ctx->uc) return MACWI_ERROR_INVALID_PARAM;

    /* Set up stack pointer */
    if (stack_top) {
        macwi_emu_reg_write(ctx, 7 /* ESP */, stack_top);
    }

    /* Start execution. 0 implies run until unmapped memory or explicit stop */
    uc_err err = uc_emu_start(ctx->uc, entry_point, 0, 0, 0);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[macwi_emu] Emulation failed at IP 0x%08X: %s\n", entry_point, uc_strerror(err));
        return MACWI_ERROR_UNSUPPORTED;
    }

    return MACWI_SUCCESS;
}

struct hook_wrapper {
    EMU_CONTEXT* ctx;
    macwi_emu_hook_cb user_cb;
    void* user_data;
};

static void uc_hook_code_cb(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    (void)uc;
    struct hook_wrapper* hw = (struct hook_wrapper*)user_data;
    if (hw && hw->user_cb) {
        hw->user_cb(hw->ctx, (uint32_t)address, size, hw->user_data);
    }
}

macwi_status_t macwi_emu_add_code_hook(EMU_CONTEXT* ctx, uint32_t begin, uint32_t end, macwi_emu_hook_cb callback, void* user_data, void** out_hook_handle) {
    if (!ctx || !ctx->uc || !callback) return MACWI_ERROR_INVALID_PARAM;

    struct hook_wrapper* hw = (struct hook_wrapper*)malloc(sizeof(struct hook_wrapper));
    if (!hw) return MACWI_ERROR_MEMORY;

    hw->ctx = ctx;
    hw->user_cb = callback;
    hw->user_data = user_data;

    uc_hook hh;
    uc_err err = uc_hook_add(ctx->uc, &hh, UC_HOOK_CODE, uc_hook_code_cb, hw, begin, end);
    if (err != UC_ERR_OK) {
        fprintf(stderr, "[macwi_emu] Failed to add code hook [%x-%x]: %s\n", begin, end, uc_strerror(err));
        free(hw);
        return MACWI_ERROR_IO;
    }

    if (out_hook_handle) {
        *out_hook_handle = (void*)(uintptr_t)hh;
    }

    return MACWI_SUCCESS;
}
