#include "gdi32.h"
#include "macwi/thunk.h"
#include "macwi/handle.h"
#include "cocoa_window.h"

extern HANDLE_TABLE g_macwi_handle_table;

static void win32_BeginPaint(EMU_CONTEXT* ctx) {
    uint32_t hWnd, lpPaint;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, &lpPaint);

    void* cocoa_win = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, &cocoa_win) == MACWI_SUCCESS) {
        PAINTSTRUCT_32 ps;
        ps.hdc = hWnd; // Cheat: use hWnd as HDC
        ps.fErase = 0;
        ps.rcPaint_left = 0;
        ps.rcPaint_top = 0;
        ps.rcPaint_right = 800; // Hardcoded size
        ps.rcPaint_bottom = 600;
        macwi_emu_write_memory(ctx, lpPaint, &ps, sizeof(ps));
        macwi_emu_reg_write_32(ctx, 0, ps.hdc);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_EndPaint(EMU_CONTEXT* ctx) {
    (void)ctx;
    macwi_emu_reg_write_32(ctx, 0, 1);
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_FillRect(EMU_CONTEXT* ctx) {
    uint32_t hDC, lprc, hbr;
    macwi_thunk_read_param_32(ctx, 0, &hDC);
    macwi_thunk_read_param_32(ctx, 1, &lprc);
    macwi_thunk_read_param_32(ctx, 2, &hbr);

    void* cocoa_win = NULL;
    // Cheat: hDC is hWnd
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hDC, HANDLE_TYPE_EVENT, &cocoa_win) == MACWI_SUCCESS) {
        RECT_32 rect;
        macwi_emu_read_memory(ctx, lprc, &rect, sizeof(rect));
        
        uint32_t color = 0xFFFF0000; // Red default
        if (hbr == 1) color = 0xFFFFFFFF; // White
        else if (hbr == 2) color = 0xFF000000; // Black
        
        macwi_cocoa_fill_rect(cocoa_win, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, color);
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 3);
}

static void win32_TextOutA(EMU_CONTEXT* ctx) {
    uint32_t hdc;
    int32_t x, y;
    uint32_t lpString;
    int32_t c;
    macwi_thunk_read_param_32(ctx, 0, &hdc);
    macwi_thunk_read_param_32(ctx, 1, (uint32_t*)&x);
    macwi_thunk_read_param_32(ctx, 2, (uint32_t*)&y);
    macwi_thunk_read_param_32(ctx, 3, &lpString);
    macwi_thunk_read_param_32(ctx, 4, (uint32_t*)&c);

    void* cocoa_win = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hdc, HANDLE_TYPE_EVENT, &cocoa_win) == MACWI_SUCCESS) {
        char text[256];
        macwi_thunk_read_guest_string(ctx, lpString, text, sizeof(text));
        if (c < 256) text[c] = '\0';
        
        macwi_cocoa_draw_text(cocoa_win, x, y, text, 0xFF000000); // Black text
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 5);
}

void macwi_gdi32_register_apis(void) {
    // Actually FillRect is in user32, but often grouped with GDI. We register it as user32 if needed.
    // In Win32, FillRect is in user32.dll!
    macwi_thunk_register_api("user32.dll", "BeginPaint", win32_BeginPaint, 2);
    macwi_thunk_register_api("user32.dll", "EndPaint", win32_EndPaint, 2);
    macwi_thunk_register_api("user32.dll", "FillRect", win32_FillRect, 3);
    macwi_thunk_register_api("gdi32.dll", "TextOutA", win32_TextOutA, 5);
}
