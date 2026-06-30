#include "gdi32.h"
#include "macwi/thunk.h"
#include "macwi/handle.h"
#include "cocoa_window.h"
#include <stdlib.h>

extern HANDLE_TABLE g_macwi_handle_table;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void win32_BeginPaint(EMU_CONTEXT* ctx) {
    uint32_t hWnd, lpPaint;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, &lpPaint);

    void** win_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
        
        MACWI_HDC_OBJ* hdc_obj = (MACWI_HDC_OBJ*)calloc(1, sizeof(MACWI_HDC_OBJ));
        hdc_obj->cocoa_window = win_obj[0]; // cocoa_win is first element of MACWI_WINDOW_OBJ
        hdc_obj->text_color = 0xFF000000; // default black
        hdc_obj->bk_color = 0xFFFFFFFF; // default white
        
        HANDLE hHdc = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_HDC, hdc_obj);
        
        printf("[macwi:gdi32] BeginPaint called hWnd=%x, returned HDC=%x\n", hWnd, (uint32_t)(uintptr_t)hHdc);
        fflush(stdout);
        
        PAINTSTRUCT_32 ps;
        ps.hdc = (uint32_t)(uintptr_t)hHdc;
        ps.fErase = 0;
        ps.rcPaint_left = 0;
        ps.rcPaint_top = 0;
        
        // Use hardcoded defaults to avoid dispatch_sync deadlock.
        // drawRect blocks the main thread waiting for EndPaint, so calling
        // macwi_cocoa_get_client_rect (which dispatch_syncs to main) here
        // would deadlock.
        ps.rcPaint_right = 800;
        ps.rcPaint_bottom = 600;
        
        macwi_emu_write_memory(ctx, lpPaint, &ps, sizeof(ps));
        macwi_emu_reg_write_32(ctx, 0, ps.hdc);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_EndPaint(EMU_CONTEXT* ctx) {
    uint32_t hWnd, lpPaint;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, &lpPaint);

    PAINTSTRUCT_32 ps;
    macwi_emu_read_memory(ctx, lpPaint, &ps, sizeof(ps));
    
    HANDLE hHdc = (HANDLE)(uintptr_t)ps.hdc;
    
    MACWI_HDC_OBJ* hdc_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, hHdc, HANDLE_TYPE_HDC, (void**)&hdc_obj) == MACWI_SUCCESS) {
        free(hdc_obj);
        macwi_handle_close(&g_macwi_handle_table, hHdc);
        
        printf("[macwi:gdi32] EndPaint called hWnd=%x\n", hWnd);
        fflush(stdout);
        
        // Unblock UI Thread
        macwi_cocoa_end_paint();
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }

    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_GetStockObject(EMU_CONTEXT* ctx) {
    uint32_t fnObject;
    macwi_thunk_read_param_32(ctx, 0, &fnObject);
    
    MACWI_GDI_OBJ* obj = (MACWI_GDI_OBJ*)calloc(1, sizeof(MACWI_GDI_OBJ));
    obj->type = GDI_OBJ_BRUSH;
    
    switch (fnObject) {
        case WHITE_BRUSH: obj->argb = 0xFFFFFFFF; break;
        case LTGRAY_BRUSH: obj->argb = 0xFFC0C0C0; break;
        case GRAY_BRUSH: obj->argb = 0xFF808080; break;
        case DKGRAY_BRUSH: obj->argb = 0xFF404040; break;
        case BLACK_BRUSH: obj->argb = 0xFF000000; break;
        case NULL_BRUSH: obj->argb = 0x00000000; break;
        default: obj->argb = 0xFF000000; break;
    }
    
    HANDLE hObj = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_GDI_OBJ, obj);
    macwi_emu_reg_write_32(ctx, 0, (uint32_t)(uintptr_t)hObj);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_SelectObject(EMU_CONTEXT* ctx) {
    uint32_t hdc, hgdiobj;
    macwi_thunk_read_param_32(ctx, 0, &hdc);
    macwi_thunk_read_param_32(ctx, 1, &hgdiobj);
    
    MACWI_HDC_OBJ* hdc_obj = NULL;
    MACWI_GDI_OBJ* gdi_obj = NULL;
    
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hdc, HANDLE_TYPE_HDC, (void**)&hdc_obj) == MACWI_SUCCESS &&
        macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hgdiobj, HANDLE_TYPE_GDI_OBJ, (void**)&gdi_obj) == MACWI_SUCCESS) {
        
        HANDLE old_obj = 0;
        if (gdi_obj->type == GDI_OBJ_BRUSH) {
            old_obj = hdc_obj->current_brush;
            hdc_obj->current_brush = (HANDLE)(uintptr_t)hgdiobj;
        } else if (gdi_obj->type == GDI_OBJ_PEN) {
            old_obj = hdc_obj->current_pen;
            hdc_obj->current_pen = (HANDLE)(uintptr_t)hgdiobj;
        } else if (gdi_obj->type == GDI_OBJ_FONT) {
            old_obj = hdc_obj->current_font;
            hdc_obj->current_font = (HANDLE)(uintptr_t)hgdiobj;
        }
        macwi_emu_reg_write_32(ctx, 0, (uint32_t)(uintptr_t)old_obj);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_CreateSolidBrush(EMU_CONTEXT* ctx) {
    uint32_t color;
    macwi_thunk_read_param_32(ctx, 0, &color);
    
    // Windows color is COLORREF: 0x00bbggrr
    uint8_t r = color & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = (color >> 16) & 0xFF;
    uint32_t argb = 0xFF000000 | (r << 16) | (g << 8) | b;
    
    MACWI_GDI_OBJ* obj = (MACWI_GDI_OBJ*)calloc(1, sizeof(MACWI_GDI_OBJ));
    obj->type = GDI_OBJ_BRUSH;
    obj->argb = argb;
    
    HANDLE hObj = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_GDI_OBJ, obj);
    macwi_emu_reg_write_32(ctx, 0, (uint32_t)(uintptr_t)hObj);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_CreateFontA(EMU_CONTEXT* ctx) {
    int32_t cHeight;
    uint32_t pszFaceName;
    macwi_thunk_read_param_32(ctx, 0, (uint32_t*)&cHeight);
    macwi_thunk_read_param_32(ctx, 13, &pszFaceName); // pszFaceName is the 14th argument (index 13)

    MACWI_GDI_OBJ* obj = (MACWI_GDI_OBJ*)calloc(1, sizeof(MACWI_GDI_OBJ));
    obj->type = GDI_OBJ_FONT;
    
    // Windows fonts use negative heights for point sizes, usually.
    if (cHeight < 0) cHeight = -cHeight;
    if (cHeight == 0) cHeight = 12; // Default size
    obj->font_size = cHeight;
    
    if (pszFaceName != 0) {
        macwi_thunk_read_guest_string(ctx, pszFaceName, obj->font_name, sizeof(obj->font_name));
    } else {
        strcpy(obj->font_name, "Arial");
    }
    
    printf("[macwi:gdi32] CreateFontA height=%d, face='%s'\n", cHeight, obj->font_name);
    fflush(stdout);

    HANDLE hObj = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_GDI_OBJ, obj);
    macwi_emu_reg_write_32(ctx, 0, (uint32_t)(uintptr_t)hObj);
    macwi_thunk_stdcall_return(ctx, 14);
}


static void win32_DeleteObject(EMU_CONTEXT* ctx) {
    uint32_t hObject;
    macwi_thunk_read_param_32(ctx, 0, &hObject);
    
    MACWI_GDI_OBJ* obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hObject, HANDLE_TYPE_GDI_OBJ, (void**)&obj) == MACWI_SUCCESS) {
        free(obj);
        macwi_handle_close(&g_macwi_handle_table, (HANDLE)(uintptr_t)hObject);
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_FillRect(EMU_CONTEXT* ctx) {
    uint32_t hDC, lprc, hbr;
    macwi_thunk_read_param_32(ctx, 0, &hDC);
    macwi_thunk_read_param_32(ctx, 1, &lprc);
    macwi_thunk_read_param_32(ctx, 2, &hbr);
    printf("[macwi:gdi32] FillRect called hDC=%u, lprc=%x, hbr=%u\n", hDC, lprc, hbr); fflush(stdout);
    
    MACWI_HDC_OBJ* hdc_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hDC, HANDLE_TYPE_HDC, (void**)&hdc_obj) == MACWI_SUCCESS) {
        RECT_32 rect;
        macwi_emu_read_memory(ctx, lprc, &rect, sizeof(rect));
        
        uint32_t color = 0xFFFFFFFF;
        MACWI_GDI_OBJ* brush = NULL;
        if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hbr, HANDLE_TYPE_GDI_OBJ, (void**)&brush) == MACWI_SUCCESS) {
            color = brush->argb;
        } else {
            // It might be a system color index + 1
            if (hbr == 1) color = 0xFFFFFFFF; // COLOR_WINDOW
            else if (hbr == 2) color = 0xFF000000;
        }
        
        macwi_cocoa_fill_rect(hdc_obj->cocoa_window, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, color);
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 3);
}

uint64_t host_CreateDIBSection(EMU_CONTEXT* ctx, uint64_t hdc, uint64_t pbmi, uint32_t usage, uint64_t ppvBits, uint64_t hSection, uint32_t offset) {
    printf("[macwi:gdi32] CreateDIBSection called\n");
    
    // Simplistic stub: Just allocate some memory for the bits and return a fake HBITMAP
    // A real implementation would parse BITMAPINFO from `pbmi` to get width/height and allocate width*height*4 bytes.
    uint32_t width = 800;
    uint32_t height = 600;
    uint32_t size = width * height * 4;
    
    // Allocate memory in the guest for the bits
    // We would need a way to allocate guest memory here, or we can just malloc and return it if we are hacking it.
    // For now, let's just write 0 to ppvBits to indicate no memory is mapped, but return a fake handle.
    if (ppvBits) {
        macwi_emu_write_memory(ctx, ppvBits, &size, 4); // Write some fake address
    }
    
    MACWI_GDI_OBJ* obj = (MACWI_GDI_OBJ*)calloc(1, sizeof(MACWI_GDI_OBJ));
    obj->type = GDI_OBJ_BITMAP;
    
    HANDLE hObj = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_GDI_OBJ, obj);
    return (uint64_t)hObj;
}

uint32_t host_BitBlt(EMU_CONTEXT* ctx, uint64_t hdcDest, uint32_t nXDest, uint32_t nYDest, uint32_t nWidth, uint32_t nHeight, uint64_t hdcSrc, uint32_t nXSrc, uint32_t nYSrc, uint32_t dwRop) {
    printf("[macwi:gdi32] BitBlt called\n");
    return 1;
}

static void win32_SetTextColor(EMU_CONTEXT* ctx) {
    uint32_t hdc, color;
    macwi_thunk_read_param_32(ctx, 0, &hdc);
    macwi_thunk_read_param_32(ctx, 1, &color);
    
    MACWI_HDC_OBJ* hdc_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hdc, HANDLE_TYPE_HDC, (void**)&hdc_obj) == MACWI_SUCCESS) {
        uint32_t old_color = hdc_obj->text_color;
        // BGR to RGB (or ARGB)
        uint32_t r = color & 0xFF;
        uint32_t g = (color >> 8) & 0xFF;
        uint32_t b = (color >> 16) & 0xFF;
        hdc_obj->text_color = (0xFF000000) | (r << 16) | (g << 8) | b;
        macwi_emu_reg_write_32(ctx, 0, old_color);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0xFFFFFFFF);
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_SetBkMode(EMU_CONTEXT* ctx) {
    uint32_t hdc, mode;
    macwi_thunk_read_param_32(ctx, 0, &hdc);
    macwi_thunk_read_param_32(ctx, 1, &mode);
    
    MACWI_HDC_OBJ* hdc_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hdc, HANDLE_TYPE_HDC, (void**)&hdc_obj) == MACWI_SUCCESS) {
        uint32_t old_mode = hdc_obj->bk_mode;
        hdc_obj->bk_mode = mode;
        macwi_emu_reg_write_32(ctx, 0, old_mode);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_Rectangle(EMU_CONTEXT* ctx) {
    uint32_t hdc;
    int32_t left, top, right, bottom;
    macwi_thunk_read_param_32(ctx, 0, &hdc);
    macwi_thunk_read_param_32(ctx, 1, (uint32_t*)&left);
    macwi_thunk_read_param_32(ctx, 2, (uint32_t*)&top);
    macwi_thunk_read_param_32(ctx, 3, (uint32_t*)&right);
    macwi_thunk_read_param_32(ctx, 4, (uint32_t*)&bottom);

    MACWI_HDC_OBJ* hdc_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hdc, HANDLE_TYPE_HDC, (void**)&hdc_obj) == MACWI_SUCCESS) {
        uint32_t color = 0xFFFFFFFF; // default white
        if (hdc_obj->current_brush) {
            MACWI_GDI_OBJ* brush = NULL;
            if (macwi_handle_get_object(&g_macwi_handle_table, hdc_obj->current_brush, HANDLE_TYPE_GDI_OBJ, (void**)&brush) == MACWI_SUCCESS) {
                color = brush->argb;
            }
        }
        // Fill rect
        macwi_cocoa_fill_rect(hdc_obj->cocoa_window, left, top, right - left, bottom - top, color);
        // Note: Real GDI draws an outline with the pen, but here we just fill for simplicity.
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 5);
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

    MACWI_HDC_OBJ* hdc_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hdc, HANDLE_TYPE_HDC, (void**)&hdc_obj) == MACWI_SUCCESS) {
        char text[256];
        macwi_thunk_read_guest_string(ctx, lpString, text, sizeof(text));
        if (c < 256) text[c] = '\0';
        
        const char* f_name = NULL;
        int f_size = 0;
        if (hdc_obj->current_font) {
            MACWI_GDI_OBJ* font = NULL;
            if (macwi_handle_get_object(&g_macwi_handle_table, hdc_obj->current_font, HANDLE_TYPE_GDI_OBJ, (void**)&font) == MACWI_SUCCESS) {
                f_name = font->font_name;
                f_size = font->font_size;
            }
        }
        
        macwi_cocoa_draw_text(hdc_obj->cocoa_window, x, y, text, hdc_obj->text_color, f_name, f_size);
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 5);
}

extern void fexi_register_gdi32(void);

void macwi_gdi32_register_apis(void) {
    fexi_register_gdi32();

    macwi_thunk_register_api("user32.dll", "BeginPaint", win32_BeginPaint, 2);
    macwi_thunk_register_api("user32.dll", "EndPaint", win32_EndPaint, 2);
    macwi_thunk_register_api("user32.dll", "FillRect", win32_FillRect, 3);
    
    macwi_thunk_register_api("gdi32.dll", "GetStockObject", win32_GetStockObject, 1);
    macwi_thunk_register_api("gdi32.dll", "SelectObject", win32_SelectObject, 2);
    macwi_thunk_register_api("gdi32.dll", "CreateSolidBrush", win32_CreateSolidBrush, 1);
    macwi_thunk_register_api("gdi32.dll", "CreateFontA", win32_CreateFontA, 14);
    macwi_thunk_register_api("gdi32.dll", "SetTextColor", win32_SetTextColor, 2);
    macwi_thunk_register_api("gdi32.dll", "SetBkMode", win32_SetBkMode, 2);
    macwi_thunk_register_api("gdi32.dll", "TextOutA", win32_TextOutA, 5);
    macwi_thunk_register_api("gdi32.dll", "Rectangle", win32_Rectangle, 5);
    macwi_thunk_register_api("gdi32.dll", "DeleteObject", win32_DeleteObject, 1);
}
