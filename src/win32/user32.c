/**
 * @file user32.c
 * @brief Win32 user32.dll stub implementations using the Cocoa bridge.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/types.h"
#include "macwi/emu.h"
#include "macwi/thunk.h"
#include "macwi/win32_structs.h"
#include "macwi/cocoa_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define USER32_LOG(fmt, ...) fprintf(stderr, "[macwi:user32] " fmt "\n", ##__VA_ARGS__)

/* We will store a global reference to the main window for simplicity initially */
static macwi_window_t g_main_window = NULL;

static void win32_MessageBoxA(EMU_CONTEXT* ctx) {
    uint64_t hWnd, lpText, lpCaption, uType;
    macwi_thunk_read_param_64(ctx, 0, &hWnd);
    macwi_thunk_read_param_64(ctx, 1, &lpText);
    macwi_thunk_read_param_64(ctx, 2, &lpCaption);
    macwi_thunk_read_param_64(ctx, 3, &uType);

    char text[1024] = {0};
    char caption[256] = {0};
    
    if (lpText) macwi_thunk_read_guest_string(ctx, lpText, text, sizeof(text));
    if (lpCaption) macwi_thunk_read_guest_string(ctx, lpCaption, caption, sizeof(caption));

    USER32_LOG("MessageBoxA: [%s] %s", caption, text);
    
    macwi_cocoa_message_box(caption, text);
    
    macwi_emu_reg_write_64(ctx, 0, 1); // IDOK
    macwi_thunk_stdcall_return(ctx, 4);
}

static void win32_RegisterClassExA(EMU_CONTEXT* ctx) {
    uint64_t lpwcx;
    macwi_thunk_read_param_64(ctx, 0, &lpwcx);
    
    if (lpwcx) {
        WNDCLASSEXA_32 wc32;
        macwi_emu_read_memory(ctx, lpwcx, &wc32, sizeof(WNDCLASSEXA_32));
        
        char class_name[256] = {0};
        if (wc32.lpszClassName) {
            macwi_thunk_read_guest_string(ctx, wc32.lpszClassName, class_name, sizeof(class_name));
        }
        USER32_LOG("RegisterClassExA('%s')", class_name);
    } else {
        USER32_LOG("RegisterClassExA(NULL)");
    }
    
    macwi_emu_reg_write_64(ctx, 0, 0x1234); // Pseudo ATOM
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_CreateWindowExA(EMU_CONTEXT* ctx) {
    uint64_t dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight;
    uint64_t hWndParent, hMenu, hInstance, lpParam;
    
    macwi_thunk_read_param_64(ctx, 0, &dwExStyle);
    macwi_thunk_read_param_64(ctx, 1, &lpClassName);
    macwi_thunk_read_param_64(ctx, 2, &lpWindowName);
    macwi_thunk_read_param_64(ctx, 3, &dwStyle);
    macwi_thunk_read_param_64(ctx, 4, &x);
    macwi_thunk_read_param_64(ctx, 5, &y);
    macwi_thunk_read_param_64(ctx, 6, &nWidth);
    macwi_thunk_read_param_64(ctx, 7, &nHeight);
    macwi_thunk_read_param_64(ctx, 8, &hWndParent);
    macwi_thunk_read_param_64(ctx, 9, &hMenu);
    macwi_thunk_read_param_64(ctx, 10, &hInstance);
    macwi_thunk_read_param_64(ctx, 11, &lpParam);

    char title[256] = "MacWI App";
    if (lpWindowName) macwi_thunk_read_guest_string(ctx, lpWindowName, title, sizeof(title));

    USER32_LOG("CreateWindowExA('%s')", title);
    
    int cx = (int)x; if (cx < 0 || cx > 10000) cx = 100;
    int cy = (int)y; if (cy < 0 || cy > 10000) cy = 100;
    int cw = (int)nWidth; if (cw <= 0 || cw > 10000) cw = 640;
    int ch = (int)nHeight; if (ch <= 0 || ch > 10000) ch = 480;

    g_main_window = macwi_cocoa_create_window(title, cx, cy, cw, ch);
    
    macwi_emu_reg_write_64(ctx, 0, (uint64_t)(uintptr_t)g_main_window); // HWND
    macwi_thunk_stdcall_return(ctx, 12);
}

static void win32_ShowWindow(EMU_CONTEXT* ctx) {
    uint64_t hWnd, nCmdShow;
    macwi_thunk_read_param_64(ctx, 0, &hWnd);
    macwi_thunk_read_param_64(ctx, 1, &nCmdShow);
    
    USER32_LOG("ShowWindow(0x%llX, %u)", hWnd, (uint32_t)nCmdShow);
    
    if (hWnd == (uint64_t)(uintptr_t)g_main_window) {
        macwi_cocoa_show_window(g_main_window, nCmdShow != 0); // 0 is SW_HIDE
    }
    macwi_emu_reg_write_64(ctx, 0, 1);
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_UpdateWindow(EMU_CONTEXT* ctx) {
    uint64_t hWnd;
    macwi_thunk_read_param_64(ctx, 0, &hWnd);
    USER32_LOG("UpdateWindow(0x%llX)", hWnd);
    macwi_emu_reg_write_64(ctx, 0, 1);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_GetMessageA(EMU_CONTEXT* ctx) {
    uint64_t lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax;
    macwi_thunk_read_param_64(ctx, 0, &lpMsg);
    macwi_thunk_read_param_64(ctx, 1, &hWnd);
    macwi_thunk_read_param_64(ctx, 2, &wMsgFilterMin);
    macwi_thunk_read_param_64(ctx, 3, &wMsgFilterMax);

    usleep(10000); 
    
    MSG_32 dummy_msg = {0};
    dummy_msg.hwnd = (uint32_t)hWnd;
    dummy_msg.message = 0; // WM_NULL
    macwi_emu_write_memory(ctx, lpMsg, &dummy_msg, sizeof(MSG_32));
    
    macwi_emu_reg_write_64(ctx, 0, 1); // Return non-zero to keep loop going
    macwi_thunk_stdcall_return(ctx, 4);
}

static void win32_TranslateMessage(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write_64(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_DispatchMessageA(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write_64(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_DefWindowProcA(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write_64(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 4);
}

static void win32_PostQuitMessage(EMU_CONTEXT* ctx) {
    uint64_t nExitCode;
    macwi_thunk_read_param_64(ctx, 0, &nExitCode);
    USER32_LOG("PostQuitMessage(%u)", (uint32_t)nExitCode);
    exit((int)nExitCode);
}

void macwi_user32_register_apis(void) {
    macwi_thunk_register_api("user32.dll", "MessageBoxA", win32_MessageBoxA, 4);
    macwi_thunk_register_api("user32.dll", "RegisterClassExA", win32_RegisterClassExA, 1);
    macwi_thunk_register_api("user32.dll", "CreateWindowExA", win32_CreateWindowExA, 12);
    macwi_thunk_register_api("user32.dll", "ShowWindow", win32_ShowWindow, 2);
    macwi_thunk_register_api("user32.dll", "UpdateWindow", win32_UpdateWindow, 1);
    macwi_thunk_register_api("user32.dll", "GetMessageA", win32_GetMessageA, 4);
    macwi_thunk_register_api("user32.dll", "TranslateMessage", win32_TranslateMessage, 1);
    macwi_thunk_register_api("user32.dll", "DispatchMessageA", win32_DispatchMessageA, 1);
    macwi_thunk_register_api("user32.dll", "DefWindowProcA", win32_DefWindowProcA, 4);
    macwi_thunk_register_api("user32.dll", "PostQuitMessage", win32_PostQuitMessage, 1);
}
