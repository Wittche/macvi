#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Simplified Windows MSG structure
struct WinMSG {
    uint64_t hwnd;
    uint32_t message;
    uint64_t wParam;
    uint64_t lParam;
    uint32_t time;
    int32_t pt_x;
    int32_t pt_y;
};

void* Metal_CreateDevice();
uint64_t Metal_CreateWindow(uint64_t hwndParent, uint32_t style, const char* title, int x, int y, int w, int h);
void* Metal_CreateLayer(uint64_t window_handle);
void Metal_ClearAndPresent(void* layer, float r, float g, float b, float a);
int Metal_PollEvents();
void PumpMacEvents();

// NEW: GDI -> Metal Translation Layer
void* Metal_GetDC(uint64_t hwnd);
void Metal_ReleaseDC(uint64_t hwnd, void* hdc);
void* Metal_BeginPaint(uint64_t hwnd, void* ps);
void Metal_EndPaint(uint64_t hwnd, void* ps);
void Metal_ExtTextOutW(void* hdc, int x, int y, uint32_t options, void* lprect, const uint16_t* text, int len, const int* dx);
void Metal_FillRect(void* hdc, void* rect, uint32_t color);
void Metal_SetTextColor(void* hdc, uint32_t color);
void Metal_SetBkColor(void* hdc, uint32_t color);
void Metal_SetBkMode(void* hdc, int mode);
void Metal_PresentDC(uint64_t hwnd, void* hdc);

void* Metal_CreateBuffer(void* device, const void* data, uint32_t size);
void* Metal_CreateVertexShader(void* device, const void* source, uint32_t size);
void* Metal_CreatePixelShader(void* device, const void* source, uint32_t size);
void Metal_SetVertexBuffer(void* context, void* buffer, uint32_t stride, uint32_t offset);
void Metal_SetConstantBuffer(void* context, uint32_t stage, uint32_t slot, void* buffer);
void Metal_SetTexture(void* context, uint32_t stage, uint32_t slot, void* texture);
void Metal_Draw(void* context, uint32_t vertexCount, uint32_t startVertexLocation);

// NEW: Proton Synchronization Helpers
void* Metal_CreateSemaphore();
void Metal_SignalSemaphore(void* sem_ptr);
void Metal_WaitSemaphore(void* sem_ptr);

// NEW: Cocoa Winecfg Window Bridge
void FEX_ShowWinecfgWindow();
bool FEX_IsWinecfgWindowOpen();

#ifdef __cplusplus
}
#endif

