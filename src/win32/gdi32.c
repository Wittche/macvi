/**
 * @file gdi32.c
 * @brief GDI32.dll emulation.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/gdi32.h"
#include "macwi/thunk.h"
#include "macwi/emu.h"

#include <stdio.h>
#include <stdlib.h>

#define GDI32_LOG(fmt, ...) fprintf(stderr, "[macwi:gdi32] " fmt "\n", ##__VA_ARGS__)

/* ============================================================================
 * Stubs
 * ============================================================================ */

static void win32_GetDeviceCaps(EMU_CONTEXT* ctx) {
    uint64_t hdc, nIndex;
    macwi_thunk_read_param_64(ctx, 0, &hdc);
    macwi_thunk_read_param_64(ctx, 1, &nIndex);
    
    GDI32_LOG("GetDeviceCaps(hdc=0x%llX, index=%u)", hdc, (uint32_t)nIndex);
    // 8 = BITSPIXEL, 12 = HORZRES, 10 = VERTRES
    uint64_t result = 0;
    if (nIndex == 8) result = 32; // 32 bits per pixel
    else if (nIndex == 12) result = 800; // HORZRES
    else if (nIndex == 10) result = 600; // VERTRES
    macwi_emu_reg_write_64(ctx, 0, result);
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_CreateCompatibleDC(EMU_CONTEXT* ctx) {
    uint64_t hdc;
    macwi_thunk_read_param_64(ctx, 0, &hdc);
    
    GDI32_LOG("CreateCompatibleDC(0x%llX)", hdc);
    macwi_emu_reg_write_64(ctx, 0, 0x2001); // Fake Memory DC
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_CreateDIBSection(EMU_CONTEXT* ctx) {
    uint64_t hdc, pbmi, usage, ppvBits, hSection, offset;
    macwi_thunk_read_param_64(ctx, 0, &hdc);
    macwi_thunk_read_param_64(ctx, 1, &pbmi);
    macwi_thunk_read_param_64(ctx, 2, &usage);
    macwi_thunk_read_param_64(ctx, 3, &ppvBits);
    macwi_thunk_read_param_64(ctx, 4, &hSection);
    macwi_thunk_read_param_64(ctx, 5, &offset);

    GDI32_LOG("CreateDIBSection(hdc=0x%llX, ...)", hdc);
    macwi_emu_reg_write_64(ctx, 0, 0x2002); // Fake Bitmap Handle
    macwi_thunk_stdcall_return(ctx, 6);
}

static void win32_SelectObject(EMU_CONTEXT* ctx) {
    uint64_t hdc, hgdiobj;
    macwi_thunk_read_param_64(ctx, 0, &hdc);
    macwi_thunk_read_param_64(ctx, 1, &hgdiobj);
    
    GDI32_LOG("SelectObject(hdc=0x%llX, obj=0x%llX)", hdc, hgdiobj);
    macwi_emu_reg_write_64(ctx, 0, 0x2003); // Fake Old Object Handle
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_BitBlt(EMU_CONTEXT* ctx) {
    uint64_t hdc, x, y, cx, cy, hdcSrc, x1, y1, rop;
    macwi_thunk_read_param_64(ctx, 0, &hdc);
    macwi_thunk_read_param_64(ctx, 1, &x);
    macwi_thunk_read_param_64(ctx, 2, &y);
    macwi_thunk_read_param_64(ctx, 3, &cx);
    macwi_thunk_read_param_64(ctx, 4, &cy);
    macwi_thunk_read_param_64(ctx, 5, &hdcSrc);
    macwi_thunk_read_param_64(ctx, 6, &x1);
    macwi_thunk_read_param_64(ctx, 7, &y1);
    macwi_thunk_read_param_64(ctx, 8, &rop);

    GDI32_LOG("BitBlt(dst=0x%llX, src=0x%llX)", hdc, hdcSrc);
    macwi_emu_reg_write_64(ctx, 0, 1); // TRUE
    macwi_thunk_stdcall_return(ctx, 9);
}

/* ============================================================================
 * API Registration
 * ============================================================================ */

void macwi_gdi32_register_apis(void) {
    macwi_thunk_register_api("gdi32.dll", "GetDeviceCaps", win32_GetDeviceCaps, 2);
    macwi_thunk_register_api("gdi32.dll", "CreateCompatibleDC", win32_CreateCompatibleDC, 1);
    macwi_thunk_register_api("gdi32.dll", "CreateDIBSection", win32_CreateDIBSection, 6);
    macwi_thunk_register_api("gdi32.dll", "SelectObject", win32_SelectObject, 2);
    macwi_thunk_register_api("gdi32.dll", "BitBlt", win32_BitBlt, 9);
}
