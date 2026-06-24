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

    size_t i = 0;
    while (i < max_len - 1) {
        char c;
        macwi_status_t status = macwi_emu_read_memory(ctx, guest_addr + i, &c, 1);
        if (status != MACWI_SUCCESS) return status;
        
        out_buf[i] = c;
        if (c == '\0') break;
        i++;
    }
    out_buf[max_len - 1] = '\0'; // ensure null termination

    return MACWI_SUCCESS;
}

macwi_status_t macwi_thunk_stdcall_return(EMU_CONTEXT* ctx, int param_count) {
    /* Handled by dispatcher, but could be exposed here if native funcs want manual control */
    (void)ctx;
    (void)param_count;
    return MACWI_SUCCESS;
}
