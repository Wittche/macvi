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
    uint32_t user_data;
    uint32_t style;
    uint32_t ex_style;
    uint32_t id;
    uint32_t parent_hwnd;
} MACWI_WINDOW_OBJ;

static void win32_RegisterClassExA(EMU_CONTEXT* ctx) {
    uint32_t lpwcx;
    macwi_thunk_read_param_32(ctx, 0, &lpwcx);

    printf("[macwi:user32] RegisterClassExA called with lpwcx=%x\n", lpwcx);
    fflush(stdout);

    WNDCLASSEXA_32 wcx;
    macwi_emu_read_memory(ctx, lpwcx, &wcx, sizeof(wcx));

    char class_name[128];
    macwi_thunk_read_guest_string(ctx, wcx.lpszClassName, class_name, sizeof(class_name));

    if (g_class_count < MAX_CLASSES) {
        strncpy(g_classes[g_class_count].class_name, class_name, 127);
        g_classes[g_class_count].wnd_proc = wcx.lpfnWndProc;
        printf("[macwi:user32] Registered class: '%s' with wnd_proc: %x\n", class_name, wcx.lpfnWndProc);
        fflush(stdout);
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

    void* cocoa_win = NULL;
    if ((dwStyle & 0x40000000) != 0) { // WS_CHILD
        MACWI_WINDOW_OBJ* parent_obj = NULL;
        if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWndParent, HANDLE_TYPE_EVENT, (void**)&parent_obj) == MACWI_SUCCESS) {
            cocoa_win = macwi_cocoa_create_child_view(parent_obj->cocoa_win, X, Y, nWidth, nHeight);
        }
    } else {
        cocoa_win = macwi_cocoa_create_window(window_name, nWidth, nHeight);
    }
    if (!cocoa_win) {
        macwi_emu_reg_write_32(ctx, 0, 0);
        macwi_thunk_stdcall_return(ctx, 12);
        return;
    }

    MACWI_WINDOW_OBJ* win_obj = malloc(sizeof(MACWI_WINDOW_OBJ));
    win_obj->cocoa_win = cocoa_win;
    win_obj->wnd_proc = 0;
    win_obj->user_data = 0;
    win_obj->style = dwStyle;
    win_obj->ex_style = dwExStyle;
    win_obj->id = (uint32_t)hMenu; // hMenu is used as child ID if child window
    win_obj->parent_hwnd = hWndParent;
    
    printf("[macwi:user32] CreateWindowExA searching for class: '%s'\n", class_name);
    fflush(stdout);

    for (int i=0; i<g_class_count; i++) {
        if (strcmp(g_classes[i].class_name, class_name) == 0) {
            win_obj->wnd_proc = g_classes[i].wnd_proc;
            printf("[macwi:user32] CreateWindowExA found class! wnd_proc: %x\n", win_obj->wnd_proc);
            fflush(stdout);
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
    printf("[macwi:user32] ShowWindow called with hwnd=%u, nCmdShow=%u\n", hWnd, nCmdShow);
    fflush(stdout);
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
        macwi_cocoa_show_window(win_obj->cocoa_win);
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

#define GWL_WNDPROC -4
#define GWL_USERDATA -21
#define GWL_STYLE -16
#define GWL_EXSTYLE -20
#define GWL_ID -12

static void win32_GetWindowLongA(EMU_CONTEXT* ctx) {
    uint32_t hWnd;
    int32_t nIndex;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, (uint32_t*)&nIndex);
    
    MACWI_WINDOW_OBJ* win_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
        uint32_t val = 0;
        if (nIndex == GWL_WNDPROC) val = win_obj->wnd_proc;
        else if (nIndex == GWL_USERDATA) val = win_obj->user_data;
        else if (nIndex == GWL_STYLE) val = win_obj->style;
        else if (nIndex == GWL_EXSTYLE) val = win_obj->ex_style;
        else if (nIndex == GWL_ID) val = win_obj->id;
        
        macwi_emu_reg_write_32(ctx, 0, val);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0); // Error
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_SetWindowLongA(EMU_CONTEXT* ctx) {
    uint32_t hWnd, dwNewLong;
    int32_t nIndex;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, (uint32_t*)&nIndex);
    macwi_thunk_read_param_32(ctx, 2, &dwNewLong);
    
    MACWI_WINDOW_OBJ* win_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
        uint32_t old_val = 0;
        if (nIndex == GWL_WNDPROC) { old_val = win_obj->wnd_proc; win_obj->wnd_proc = dwNewLong; }
        else if (nIndex == GWL_USERDATA) { old_val = win_obj->user_data; win_obj->user_data = dwNewLong; }
        else if (nIndex == GWL_STYLE) { old_val = win_obj->style; win_obj->style = dwNewLong; }
        else if (nIndex == GWL_EXSTYLE) { old_val = win_obj->ex_style; win_obj->ex_style = dwNewLong; }
        else if (nIndex == GWL_ID) { old_val = win_obj->id; win_obj->id = dwNewLong; }
        
        macwi_emu_reg_write_32(ctx, 0, old_val);
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0); // Error
    }
    macwi_thunk_stdcall_return(ctx, 3);
}

static void win32_DefWindowProcA(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write_32(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 4);
}

static void win32_PostQuitMessage(EMU_CONTEXT* ctx) {
    macwi_cocoa_post_quit_message();
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
    while (1) {
        int got_event = macwi_cocoa_poll_event(&event);
        
        if (got_event) {
            MSG_32 msg;
            memset(&msg, 0, sizeof(msg));
            
            // Find HWND
            HANDLE found_hwnd = 0;
            for (uint32_t i=0; i<g_macwi_handle_table.capacity; i++) {
                if (g_macwi_handle_table.entries[i].type == HANDLE_TYPE_EVENT && g_macwi_handle_table.entries[i].object) {
                    MACWI_WINDOW_OBJ* obj = (MACWI_WINDOW_OBJ*)g_macwi_handle_table.entries[i].object;
                    if (obj->cocoa_win == event.window) {
                        found_hwnd = (HANDLE)(uintptr_t)(((uintptr_t)g_macwi_handle_table.entries[i].generation << 16) | i);
                        break;
                    }
                }
            }
            
            msg.hwnd = (uint32_t)(uintptr_t)found_hwnd;
            
            if (event.type == MACWI_EVENT_CLOSE) {
                msg.message = WM_CLOSE;
            } else if (event.type == MACWI_EVENT_PAINT) {
                msg.message = WM_PAINT;
            } else if (event.type == MACWI_EVENT_KEYDOWN) {
                msg.message = WM_KEYDOWN;
                msg.wParam = event.key_code;
            } else if (event.type == MACWI_EVENT_KEYUP) {
                msg.message = WM_KEYUP;
                msg.wParam = event.key_code;
            } else if (event.type == MACWI_EVENT_MOUSEDOWN) {
                msg.message = WM_LBUTTONDOWN;
                msg.lParam = (event.mouse_y << 16) | (event.mouse_x & 0xFFFF);
            } else if (event.type == MACWI_EVENT_MOUSEUP) {
                msg.message = WM_LBUTTONUP;
                msg.lParam = (event.mouse_y << 16) | (event.mouse_x & 0xFFFF);
            } else if (event.type == MACWI_EVENT_QUIT) {
                msg.message = WM_QUIT;
            } else {
                msg.message = 0;
            }
            
            printf("[macwi:user32] GetMessageA retrieved event type=%d, HWND=%x, msg=%x\n", event.type, msg.hwnd, msg.message);
            fflush(stdout);
            
            macwi_emu_write_memory(ctx, lpMsg, &msg, sizeof(msg));
            macwi_emu_reg_write_32(ctx, 0, (msg.message != WM_QUIT) ? 1 : 0);
            macwi_thunk_stdcall_return(ctx, 4);
            return;
        }
        
        // Wait briefly for events
        struct timespec ts = {0, 5000000}; // 5ms
        nanosleep(&ts, NULL);
    }
}

static void win32_TranslateMessage(EMU_CONTEXT* ctx) {
    uint32_t lpMsg;
    macwi_thunk_read_param_32(ctx, 0, &lpMsg);
    
    printf("[macwi:user32] TranslateMessage called with lpMsg=%x\n", lpMsg);
    fflush(stdout);

    macwi_emu_reg_write_32(ctx, 0, 1);
    macwi_thunk_stdcall_return(ctx, 1);
}

#define BUILTIN_WNDPROC_BUTTON 0xFFFFFFF1
#define BUILTIN_WNDPROC_STATIC 0xFFFFFFF2
#define BUILTIN_WNDPROC_EDIT   0xFFFFFFF3
#define WM_COMMAND 0x0111

static void handle_builtin_wndproc(EMU_CONTEXT* ctx, MACWI_WINDOW_OBJ* win, MSG_32* msg) {
    if (msg->message == WM_PAINT) {
        int w, h;
        macwi_cocoa_get_client_rect(win->cocoa_win, &w, &h);
        
        if (win->wnd_proc == BUILTIN_WNDPROC_BUTTON) {
            // Draw Button
            macwi_cocoa_fill_rect(win->cocoa_win, 0, 0, w, h, 0xFFE0E0E0); // Light Gray
            macwi_cocoa_draw_text(win->cocoa_win, 5, 5, "Button", 0xFF000000); // Black text
        } else if (win->wnd_proc == BUILTIN_WNDPROC_STATIC) {
            macwi_cocoa_fill_rect(win->cocoa_win, 0, 0, w, h, 0xFFF0F0F0); // System background
            macwi_cocoa_draw_text(win->cocoa_win, 0, 0, "Static", 0xFF000000);
        } else if (win->wnd_proc == BUILTIN_WNDPROC_EDIT) {
            macwi_cocoa_fill_rect(win->cocoa_win, 0, 0, w, h, 0xFFFFFFFF); // White background
            macwi_cocoa_draw_text(win->cocoa_win, 2, 2, "Edit", 0xFF000000);
        }
        
        // Mark paint as finished
        macwi_cocoa_end_paint();
        macwi_emu_reg_write_32(ctx, 0, 0);
        macwi_thunk_stdcall_return(ctx, 1);
        return;
    }
    
    if (win->wnd_proc == BUILTIN_WNDPROC_BUTTON) {
        if (msg->message == WM_LBUTTONDOWN) {
            MACWI_WINDOW_OBJ* parent = NULL;
            if (win->parent_hwnd && macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)win->parent_hwnd, HANDLE_TYPE_EVENT, (void**)&parent) == MACWI_SUCCESS) {
                printf("[macwi:user32] BUTTON clicked! id=%d\n", win->id);
                fflush(stdout);
                
                // Invoke parent's wnd_proc with WM_COMMAND
                uint32_t args[4] = { win->parent_hwnd, WM_COMMAND, (win->id & 0xFFFF), (uint32_t)msg->hwnd };
                macwi_status_t st = macwi_thunk_invoke_callback(ctx, parent->wnd_proc, 4, args, 4, NULL);
                if (st == MACWI_SUCCESS) {
                    // Return without stdcall_return since we modified PC
                    return;
                }
            }
        }
    }
    // For all built-ins, return 0 (DefWindowProc)
    macwi_emu_reg_write_32(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_DispatchMessageA(EMU_CONTEXT* ctx) {
    uint32_t lpMsg;
    macwi_thunk_read_param_32(ctx, 0, &lpMsg);
    
    MSG_32 msg;
    macwi_emu_read_memory(ctx, lpMsg, &msg, sizeof(msg));
    
    printf("[macwi:user32] DispatchMessageA called with hwnd=%x, msg=%x\n", msg.hwnd, msg.message);
    fflush(stdout);

    MACWI_WINDOW_OBJ* win_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)msg.hwnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
        if (win_obj->wnd_proc >= 0xFFFFFFF0) {
            handle_builtin_wndproc(ctx, win_obj, &msg);
            return;
        } else if (win_obj->wnd_proc != 0) {
            printf("[macwi:user32] DispatchMessageA: invoking WindowProc %x with msg %x\n", win_obj->wnd_proc, msg.message);
            fflush(stdout);
            
            uint32_t args[4] = { msg.hwnd, msg.message, msg.wParam, msg.lParam };
            macwi_status_t st = macwi_thunk_invoke_callback(ctx, win_obj->wnd_proc, 4, args, 4, NULL);
            if (st == MACWI_SUCCESS) {
                // Return without doing stdcall_return since we modified the stack and PC to run the callback
                return;
            }
        }
    }
    
    macwi_emu_reg_write_32(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CXSIZEFRAME 32
#define SM_CYSIZEFRAME 33
#define SM_CYCAPTION 4

static void win32_GetSystemMetrics(EMU_CONTEXT* ctx) {
    int32_t nIndex;
    macwi_thunk_read_param_32(ctx, 0, (uint32_t*)&nIndex);
    
    int val = 0;
    if (nIndex == SM_CXSCREEN) val = 1920; // Hardcode for now, or get from Cocoa
    else if (nIndex == SM_CYSCREEN) val = 1080;
    else if (nIndex == SM_CXSIZEFRAME || nIndex == SM_CYSIZEFRAME) val = 4;
    else if (nIndex == SM_CYCAPTION) val = 22;
    
    macwi_emu_reg_write_32(ctx, 0, val);
    macwi_thunk_stdcall_return(ctx, 1);
}

#define COLOR_BTNFACE 15
#define COLOR_WINDOW 5
#define COLOR_BTNTEXT 18
#define COLOR_WINDOWTEXT 8

static void win32_GetSysColor(EMU_CONTEXT* ctx) {
    int32_t nIndex;
    macwi_thunk_read_param_32(ctx, 0, (uint32_t*)&nIndex);
    
    uint32_t color = 0; // RGB
    if (nIndex == COLOR_BTNFACE) color = 0xF0F0F0;
    else if (nIndex == COLOR_WINDOW) color = 0xFFFFFF;
    else if (nIndex == COLOR_BTNTEXT) color = 0x000000;
    else if (nIndex == COLOR_WINDOWTEXT) color = 0x000000;
    
    macwi_emu_reg_write_32(ctx, 0, color);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_SetWindowPos(EMU_CONTEXT* ctx) {
    // HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags
    macwi_emu_reg_write_32(ctx, 0, 1); // Success
    macwi_thunk_stdcall_return(ctx, 7);
}

static void win32_MoveWindow(EMU_CONTEXT* ctx) {
    // HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint
    macwi_emu_reg_write_32(ctx, 0, 1); // Success
    macwi_thunk_stdcall_return(ctx, 6);
}

static void win32_GetClientRect(EMU_CONTEXT* ctx) {
    uint32_t hWnd, lpRect;
    macwi_thunk_read_param_32(ctx, 0, &hWnd);
    macwi_thunk_read_param_32(ctx, 1, &lpRect);
    printf("[macwi:user32] GetClientRect called for hWnd=%u\n", hWnd); fflush(stdout);
    
    MACWI_WINDOW_OBJ* win_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hWnd, HANDLE_TYPE_EVENT, (void**)&win_obj) == MACWI_SUCCESS) {
        int w = 800, h = 600; // Hardcoded default for now
        // TODO: Get actual cached width and height from win_obj when implemented
        uint32_t rect[4] = {0, 0, w, h};
        macwi_emu_write_memory(ctx, lpRect, rect, sizeof(rect));
        macwi_emu_reg_write_32(ctx, 0, 1);
        printf("[macwi:user32] GetClientRect SUCCESS\n"); fflush(stdout);
    } else {
        printf("[macwi:user32] GetClientRect FAILED to find window\n"); fflush(stdout);
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
        int x = 0, y = 0, w = 800, h = 600; // Hardcoded default for now
        // TODO: Get actual cached rect from win_obj when implemented
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
    // Register builtin classes
    strncpy(g_classes[g_class_count].class_name, "BUTTON", 128);
    g_classes[g_class_count].wnd_proc = BUILTIN_WNDPROC_BUTTON;
    g_class_count++;
    
    strncpy(g_classes[g_class_count].class_name, "STATIC", 128);
    g_classes[g_class_count].wnd_proc = BUILTIN_WNDPROC_STATIC;
    g_class_count++;
    
    strncpy(g_classes[g_class_count].class_name, "EDIT", 128);
    g_classes[g_class_count].wnd_proc = BUILTIN_WNDPROC_EDIT;
    g_class_count++;

    macwi_thunk_register_api("user32.dll", "GetSystemMetrics", win32_GetSystemMetrics, 1);
    macwi_thunk_register_api("user32.dll", "GetSysColor", win32_GetSysColor, 1);
    macwi_thunk_register_api("user32.dll", "GetWindowLongA", win32_GetWindowLongA, 2);
    macwi_thunk_register_api("user32.dll", "SetWindowLongA", win32_SetWindowLongA, 3);
    macwi_thunk_register_api("user32.dll", "SetWindowPos", win32_SetWindowPos, 7);
    macwi_thunk_register_api("user32.dll", "MoveWindow", win32_MoveWindow, 6);
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
