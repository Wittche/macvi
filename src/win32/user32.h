#pragma once

#include "macwi/emu.h"

#ifdef __cplusplus
extern "C" {
#endif

// Register User32 APIs
void macwi_user32_register_apis(void);

// Win32 Type Definitions
typedef uint32_t HWND32;
typedef uint32_t HMENU32;
typedef uint32_t HINSTANCE32;
typedef uint32_t HICON32;
typedef uint32_t HCURSOR32;
typedef uint32_t HBRUSH32;
typedef uint32_t WNDPROC32;

typedef struct {
    uint32_t cbSize;
    uint32_t style;
    WNDPROC32 lpfnWndProc;
    int32_t cbClsExtra;
    int32_t cbWndExtra;
    HINSTANCE32 hInstance;
    HICON32 hIcon;
    HCURSOR32 hCursor;
    HBRUSH32 hbrBackground;
    uint32_t lpszMenuName; // ptr32
    uint32_t lpszClassName; // ptr32
    HICON32 hIconSm;
} WNDCLASSEXA_32;

typedef struct {
    int32_t x;
    int32_t y;
} POINT_32;

typedef struct {
    HWND32 hwnd;
    uint32_t message;
    uint32_t wParam;
    uint32_t lParam;
    uint32_t time;
    POINT_32 pt;
    uint32_t lPrivate;
} MSG_32;

#define WM_DESTROY      0x0002
#define WM_PAINT        0x000F
#define WM_CLOSE        0x0010
#define WM_QUIT         0x0012
#define WM_KEYDOWN      0x0100
#define WM_KEYUP        0x0101
#define WM_MOUSEMOVE    0x0200
#define WM_LBUTTONDOWN  0x0201
#define WM_LBUTTONUP    0x0202

#define SW_SHOW         5
#define SW_SHOWNORMAL   1

#ifdef __cplusplus
}
#endif
