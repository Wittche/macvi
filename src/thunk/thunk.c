/**
 * @file thunk.c
 * @brief Thunking layer — Parameter marshaling helpers.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/thunk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

macwi_status_t macwi_thunk_read_param_32(EMU_CONTEXT* ctx, int param_index, uint32_t* out_val) {
    if (!ctx || !out_val || param_index < 0) return MACWI_ERROR_INVALID_PARAM;

    uint32_t esp = 0;
    macwi_status_t status = macwi_emu_reg_read(ctx, 7 /* ESP */, &esp);
    if (status != MACWI_SUCCESS) return status;

    /*
     * ESP points to the return address.
     * ESP + 4 is param 0.
     * ESP + 8 is param 1, etc.
     */
    uint32_t param_addr = esp + 4 + (param_index * 4);
    
    return macwi_emu_read_memory(ctx, param_addr, out_val, 4);
}

macwi_status_t macwi_thunk_read_guest_string(EMU_CONTEXT* ctx, uint32_t guest_addr, char* out_buf, size_t max_len) {
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

macwi_status_t macwi_thunk_string_out(EMU_CONTEXT* ctx, uint32_t guest_addr, const char* in_str, size_t max_len) {
    if (!ctx || !in_str || max_len == 0) return MACWI_ERROR_INVALID_PARAM;
    
    size_t len = strlen(in_str);
    if (len >= max_len) {
        len = max_len - 1;
    }
    
    if (macwi_emu_write_memory(ctx, guest_addr, in_str, len) != MACWI_SUCCESS) {
        return MACWI_ERROR_MEMORY;
    }
    
    // Null terminator
    uint8_t zero = 0;
    if (macwi_emu_write_memory(ctx, guest_addr + len, &zero, 1) != MACWI_SUCCESS) {
        return MACWI_ERROR_MEMORY;
    }
    
    return MACWI_SUCCESS;
}

macwi_status_t macwi_thunk_stdcall_return(EMU_CONTEXT* ctx, int param_count) {
    /* Handled by dispatcher, but could be exposed here if native funcs want manual control */
    (void)ctx;
    (void)param_count;
    return MACWI_SUCCESS;
}
