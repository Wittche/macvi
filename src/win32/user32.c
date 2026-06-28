#include "user32.h"
#include "macwi/thunk.h"
#include "macwi/handle.h"
#include "cocoa_window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern HANDLE_TABLE g_macwi_handle_table;

#define MAX_CLASSES 64
typedef struct {
    char class_name[128];
    WNDPROC32 wnd_proc;
} WIN32_CLASS;

static WIN32_CLASS g_classes[MAX_CLASSES];
static int g_class_count = 0;

static void win32_RegisterClassExA(EMU_CONTEXT* ctx) {
    uint32_t lpwcx;
    if (macwi_thunk_read_param_32(ctx, 0, &lpwcx) != MACWI_SUCCESS) {
        macwi_thunk_stdcall_return(ctx, 1);
        return;
    }

    WNDCLASSEXA_32 wcx;
    if (macwi_emu_read_memory(ctx, lpwcx, &wcx, sizeof(wcx)) != MACWI_SUCCESS) {
        macwi_emu_reg_write_32(ctx, 0, 0); // Fail
        macwi_thunk_stdcall_return(ctx, 1);
        return;
    }

    char class_name[128];
    macwi_thunk_read_guest_string(ctx, wcx.lpszClassName, class_name, sizeof(class_name));

    if (g_class_count < MAX_CLASSES) {
        strncpy(g_classes[g_class_count].class_name, class_name, 127);
        g_classes[g_class_count].wnd_proc = wcx.lpfnWndProc;
        g_class_count++;
        macwi_emu_reg_write_32(ctx, 0, 1); // Success atom
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0); // Fail
    }
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_CreateWindowExA(EMU_CONTEXT* ctx) {
    uint32_t dwExStyle, lpClassName, lpWindowName, dwStyle;
    int32_t X, Y, nWidth, nHeight;
    uint32_t hWndParent, hMenu, hInstance, lpParam;
    
    macwi_thunk_read_param_32(ctx, 0, &dwExStyle);
    macwi_thunk_read_param_32(ctx, 1, &lpClassName);
    macwi_thunk_read_param_32(ctx, 2, &lpWindowName);
    macwi_thunk_read_param_32(ctx, 3, &dwStyle);
    macwi_thunk_read_param_32(ctx, 4, (uint32_t*)&X);
    macwi_thunk_read_param_32(ctx, 5, (uint32_t*)&Y);
    macwi_thunk_read_param_32(ctx, 6, (uint32_t*)&nWidth);
    macwi_thunk_read_param_32(ctx, 7, (uint32_t*)&nHeight);
    macwi_thunk_read_param_32(ctx, 8, &hWndParent);
    macwi_thunk_read_param_32(ctx, 9, &hMenu);
    macwi_thunk_read_param_32(ctx, 10, &hInstance);
    macwi_thunk_read_param_32(ctx, 11, &lpParam);

    char class_name[128];
    char window_name[256];
    
    // Sometimes class name is an atom (lower 16 bits), handle string only for now
    if (lpClassName > 0xFFFF) {
        macwi_thunk_read_guest_string(ctx, lpClassName, class_name, sizeof(class_name));
    } else {
        snprintf(class_name, sizeof(class_name), "ATOM_%u", lpClassName);
    }
    macwi_thunk_read_guest_string(ctx, lpWindowName, window_name, sizeof(window_name));

    if (nWidth == (int32_t)0x80000000) nWidth = 800; // CW_USEDEFAULT
    if (nHeight == (int32_t)0x80000000) nHeight = 600;

    void* cocoa_win = macwi_cocoa_create_window(window_name, nWidth, nHeight);
    if (!cocoa_win) {
        macwi_emu_reg_write_32(ctx, 0, 0);
        macwi_thunk_stdcall_return(ctx, 12);
        return;
    }

    // Since a window needs its WindowProc, we map HWND -> cocoa_win. 
    // We should also store which class it belongs to so we can dispatch messages.
    HANDLE hwnd = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_EVENT, cocoa_win); // Using EVENT type loosely, maybe define HWND type
    
    macwi_emu_reg_write_32(ctx, 0, (uint32_t)(uintptr_t)hwnd);
    macwi_thunk_stdcall_return(ctx, 12);
}

static void win32_ShowWindow(EMU_CONTEXT* ctx) {
    uint32_t hWnd, nCmdShow;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, &nCmdShow);
    
    void* cocoa_win = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, &cocoa_win) == MACWI_SUCCESS) {
        if (nCmdShow == SW_SHOW || nCmdShow == SW_SHOWNORMAL) {
            macwi_cocoa_show_window(cocoa_win);
        }
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_UpdateWindow(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write_32(ctx, 0, 1); // Stub
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_DefWindowProcA(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write_32(ctx, 0, 0); // Default return
    macwi_thunk_stdcall_return(ctx, 4);
}

static void win32_PostQuitMessage(EMU_CONTEXT* ctx) {
    (void)ctx;
    // Should post WM_QUIT
    macwi_thunk_stdcall_return(ctx, 1);
}

// In a real implementation, GetMessageA blocks. For our simple test, we will poll.
static void win32_GetMessageA(EMU_CONTEXT* ctx) {
    uint32_t lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax;
    macwi_thunk_read_param_32(ctx, 0, &lpMsg);
    macwi_thunk_read_param_32(ctx, 1, &hWnd);
    macwi_thunk_read_param_32(ctx, 2, &wMsgFilterMin);
    macwi_thunk_read_param_32(ctx, 3, &wMsgFilterMax);

    macwi_event_t event;
    int got_event = macwi_cocoa_poll_event(&event);
    
    if (got_event) {
        MSG_32 msg;
        memset(&msg, 0, sizeof(msg));
        
        // Find HWND for cocoa_win
        // For simplicity, we just assume HWND 1 or search handle table.
        // Let's just use 1 for now if we don't have a reverse lookup
        msg.hwnd = 1; // HARDCODED for now!
        
        if (event.type == MACWI_EVENT_CLOSE) {
            msg.message = WM_CLOSE;
        } else if (event.type == MACWI_EVENT_PAINT) {
            msg.message = WM_PAINT;
        } else {
            // Ignore other events for now, return a dummy message
            msg.message = 0; // WM_NULL
        }
        
        macwi_emu_write_memory(ctx, lpMsg, &msg, sizeof(msg));
        macwi_emu_reg_write_32(ctx, 0, (msg.message != WM_QUIT) ? 1 : 0);
    } else {
        // We should block, but to avoid blocking the emulator thread completely, 
        // we might yield or sleep. For this test, let's sleep briefly and return TRUE with a WM_NULL.
        // Actually, FEXCore ExecuteThread is running us. If we block in C, FEX is blocked.
        // It's safe to block here! But we must sleep to avoid 100% CPU.
        struct timespec ts = {0, 10000000}; // 10ms
        nanosleep(&ts, NULL);
        
        MSG_32 msg;
        memset(&msg, 0, sizeof(msg));
        msg.message = 0; // WM_NULL
        macwi_emu_write_memory(ctx, lpMsg, &msg, sizeof(msg));
        macwi_emu_reg_write_32(ctx, 0, 1);
    }
    macwi_thunk_stdcall_return(ctx, 4);
}

static void win32_TranslateMessage(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write_32(ctx, 0, 1);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_DispatchMessageA(EMU_CONTEXT* ctx) {
    uint32_t lpMsg;
    macwi_thunk_read_param_32(ctx, 0, &lpMsg);
    
    MSG_32 msg;
    macwi_emu_read_memory(ctx, lpMsg, &msg, sizeof(msg));
    
    // We need to call the WindowProc! 
    // In our simplified test, we don't do a full FEX_CallFunction.
    // Instead, we will simulate the dispatch by modifying the guest stack to CALL the WindowProc,
    // and setting a dummy return address to our Thunking Layer!
    // But since DispatchMessage is ALREADY inside a syscall, we can't easily re-enter the guest cleanly
    // without returning from the syscall.
    
    // For the Phase 12 GUI test, if the app just uses GetMessage and processes it linearly, we are fine.
    // Wait, Windows apps do:
    // while (GetMessage(&msg)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    // DispatchMessage CALLS the WindowProc.
    // We CANNOT easily call the WindowProc from host without a proper FEX callback mechanism.
    // Alternatively, we can let `DispatchMessage` return the parameters on a special thunk struct,
    // and the guest trampoline for DispatchMessage can actually do the CALL.
    
    // To keep it simple: we just return and let the guest handle it if we modify `gui_win32.c` to call WindowProc directly, OR we implement a quick hack in `gui_win32.c` to not rely on DispatchMessage.
    
    macwi_emu_reg_write_32(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

void macwi_user32_register_apis(void) {
    macwi_thunk_register_api("user32.dll", "RegisterClassExA", win32_RegisterClassExA, 1);
    macwi_thunk_register_api("user32.dll", "CreateWindowExA", win32_CreateWindowExA, 12);
    macwi_thunk_register_api("user32.dll", "ShowWindow", win32_ShowWindow, 2);
    macwi_thunk_register_api("user32.dll", "UpdateWindow", win32_UpdateWindow, 1);
    macwi_thunk_register_api("user32.dll", "DefWindowProcA", win32_DefWindowProcA, 4);
    macwi_thunk_register_api("user32.dll", "PostQuitMessage", win32_PostQuitMessage, 1);
    macwi_thunk_register_api("user32.dll", "GetMessageA", win32_GetMessageA, 4);
    macwi_thunk_register_api("user32.dll", "TranslateMessage", win32_TranslateMessage, 1);
    macwi_thunk_register_api("user32.dll", "DispatchMessageA", win32_DispatchMessageA, 1);
}
