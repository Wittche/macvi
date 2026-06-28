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

typedef struct {
    void* cocoa_win;
    WNDPROC32 wnd_proc;
} MACWI_WINDOW_OBJ;

static void win32_RegisterClassExA(EMU_CONTEXT* ctx) {
    uint32_t lpwcx;
    macwi_thunk_read_param_32(ctx, 0, &lpwcx);

    WNDCLASSEXA_32 wcx;
    macwi_emu_read_memory(ctx, lpwcx, &wcx, sizeof(wcx));

    char class_name[128];
    macwi_thunk_read_guest_string(ctx, wcx.lpszClassName, class_name, sizeof(class_name));

    if (g_class_count < MAX_CLASSES) {
        strncpy(g_classes[g_class_count].class_name, class_name, 127);
        g_classes[g_class_count].wnd_proc = wcx.lpfnWndProc;
        g_class_count++;
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
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
    
    if (lpClassName > 0xFFFF) {
        macwi_thunk_read_guest_string(ctx, lpClassName, class_name, sizeof(class_name));
    } else {
        snprintf(class_name, sizeof(class_name), "ATOM_%u", lpClassName);
    }
    macwi_thunk_read_guest_string(ctx, lpWindowName, window_name, sizeof(window_name));

    if (nWidth == (int32_t)0x80000000) nWidth = 800;
    if (nHeight == (int32_t)0x80000000) nHeight = 600;

    void* cocoa_win = macwi_cocoa_create_window(window_name, nWidth, nHeight);
    if (!cocoa_win) {
        macwi_emu_reg_write_32(ctx, 0, 0);
        macwi_thunk_stdcall_return(ctx, 12);
        return;
    }

    MACWI_WINDOW_OBJ* win_obj = malloc(sizeof(MACWI_WINDOW_OBJ));
    win_obj->cocoa_win = cocoa_win;
    win_obj->wnd_proc = 0;
    
    for (int i=0; i<g_class_count; i++) {
        if (strcmp(g_classes[i].class_name, class_name) == 0) {
            win_obj->wnd_proc = g_classes[i].wnd_proc;
            break;
        }
    }

    HANDLE hwnd = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_EVENT, win_obj);
    
    macwi_emu_reg_write_32(ctx, 0, (uint32_t)(uintptr_t)hwnd);
    macwi_thunk_stdcall_return(ctx, 12);
}

static void win32_ShowWindow(EMU_CONTEXT* ctx) {
    uint32_t hWnd, nCmdShow;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, &nCmdShow);
    
    MACWI_WINDOW_OBJ* win_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
        if (nCmdShow == SW_SHOW || nCmdShow == SW_SHOWNORMAL) {
            macwi_cocoa_show_window(win_obj->cocoa_win);
        }
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_UpdateWindow(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write_32(ctx, 0, 1);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_DefWindowProcA(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write_32(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 4);
}

static void win32_PostQuitMessage(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write_32(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

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
        
        // Find HWND
        HANDLE found_hwnd = 0;
        for (int i=0; i<g_macwi_handle_table.capacity; i++) {
            if (g_macwi_handle_table.entries[i].type == HANDLE_TYPE_EVENT && g_macwi_handle_table.entries[i].object) {
                MACWI_WINDOW_OBJ* obj = (MACWI_WINDOW_OBJ*)g_macwi_handle_table.entries[i].object;
                if (obj->cocoa_win == event.window) {
                    found_hwnd = (HANDLE)(uintptr_t)((i << 16) | g_macwi_handle_table.entries[i].generation);
                    break;
                }
            }
        }
        
        msg.hwnd = (uint32_t)(uintptr_t)found_hwnd;
        
        if (event.type == MACWI_EVENT_CLOSE) {
            msg.message = WM_CLOSE;
        } else if (event.type == MACWI_EVENT_PAINT) {
            msg.message = WM_PAINT;
        } else {
            msg.message = 0;
        }
        
        macwi_emu_write_memory(ctx, lpMsg, &msg, sizeof(msg));
        macwi_emu_reg_write_32(ctx, 0, (msg.message != WM_QUIT) ? 1 : 0);
    } else {
        struct timespec ts = {0, 10000000};
        nanosleep(&ts, NULL);
        
        MSG_32 msg;
        memset(&msg, 0, sizeof(msg));
        msg.message = 0;
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
    
    MACWI_WINDOW_OBJ* win_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)msg.hwnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
        if (win_obj->wnd_proc != 0) {
            uint32_t args[4] = { msg.hwnd, msg.message, msg.wParam, msg.lParam };
            macwi_status_t st = macwi_thunk_invoke_callback(ctx, win_obj->wnd_proc, 4, args, NULL);
            if (st == MACWI_SUCCESS) {
                // Return without doing stdcall_return since we modified the stack and PC to run the callback
                return;
            }
        }
    }
    
    macwi_emu_reg_write_32(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_GetClientRect(EMU_CONTEXT* ctx) {
    uint32_t hWnd, lpRect;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, &lpRect);
    
    MACWI_WINDOW_OBJ* win_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
        int w = 0, h = 0;
        macwi_cocoa_get_client_rect(win_obj->cocoa_win, &w, &h);
        uint32_t rect[4] = {0, 0, w, h};
        macwi_emu_write_memory(ctx, lpRect, rect, sizeof(rect));
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_GetWindowRect(EMU_CONTEXT* ctx) {
    uint32_t hWnd, lpRect;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, &lpRect);
    
    MACWI_WINDOW_OBJ* win_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
        int x = 0, y = 0, w = 0, h = 0;
        macwi_cocoa_get_window_rect(win_obj->cocoa_win, &x, &y, &w, &h);
        uint32_t rect[4] = {x, y, x + w, y + h};
        macwi_emu_write_memory(ctx, lpRect, rect, sizeof(rect));
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_SetWindowTextA(EMU_CONTEXT* ctx) {
    uint32_t hWnd, lpString;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, &lpString);
    
    MACWI_WINDOW_OBJ* win_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
        char text[256];
        macwi_thunk_read_guest_string(ctx, lpString, text, sizeof(text));
        macwi_cocoa_set_text(win_obj->cocoa_win, text);
        macwi_emu_reg_write_32(ctx, 0, 1);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_GetWindowTextA(EMU_CONTEXT* ctx) {
    uint32_t hWnd, lpString, nMaxCount;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, &lpString);
    macwi_thunk_read_param_32(ctx, 2, &nMaxCount);
    
    MACWI_WINDOW_OBJ* win_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
        char text[256] = {0};
        macwi_cocoa_get_text(win_obj->cocoa_win, text, sizeof(text));
        macwi_thunk_string_out(ctx, lpString, text, nMaxCount);
        macwi_emu_reg_write_32(ctx, 0, strlen(text));
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 3);
}

static void win32_MessageBoxA(EMU_CONTEXT* ctx) {
    uint32_t hWnd, lpText, lpCaption, uType;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, &lpText);
    macwi_thunk_read_param_32(ctx, 2, &lpCaption);
    macwi_thunk_read_param_32(ctx, 3, &uType);
    
    char text[512] = {0};
    char caption[256] = {0};
    
    if (lpText) macwi_thunk_read_guest_string(ctx, lpText, text, sizeof(text));
    if (lpCaption) macwi_thunk_read_guest_string(ctx, lpCaption, caption, sizeof(caption));
    
    void* cocoa_win = NULL;
    if (hWnd) {
        MACWI_WINDOW_OBJ* win_obj = NULL;
        if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
            cocoa_win = win_obj->cocoa_win;
        }
    }
    
    int ret = macwi_cocoa_message_box(cocoa_win, text, caption, uType);
    macwi_emu_reg_write_32(ctx, 0, ret);
    macwi_thunk_stdcall_return(ctx, 4);
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
    macwi_thunk_register_api("user32.dll", "GetClientRect", win32_GetClientRect, 2);
    macwi_thunk_register_api("user32.dll", "GetWindowRect", win32_GetWindowRect, 2);
    macwi_thunk_register_api("user32.dll", "SetWindowTextA", win32_SetWindowTextA, 2);
    macwi_thunk_register_api("user32.dll", "GetWindowTextA", win32_GetWindowTextA, 3);
    macwi_thunk_register_api("user32.dll", "MessageBoxA", win32_MessageBoxA, 4);
}
