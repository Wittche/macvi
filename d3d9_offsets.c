#include <windows.h>
#include <d3d9.h>
#include <stdio.h>

int main() {
    printf("--- IDirect3DDevice9 VTable Offsets ---\n");
    printf("QueryInterface: %zu\n", offsetof(IDirect3DDevice9Vtbl, QueryInterface) / sizeof(void*));
    printf("AddRef: %zu\n", offsetof(IDirect3DDevice9Vtbl, AddRef) / sizeof(void*));
    printf("Release: %zu\n", offsetof(IDirect3DDevice9Vtbl, Release) / sizeof(void*));
    printf("TestCooperativeLevel: %zu\n", offsetof(IDirect3DDevice9Vtbl, TestCooperativeLevel) / sizeof(void*));
    printf("GetAvailableTextureMem: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetAvailableTextureMem) / sizeof(void*));
    printf("EvictManagedResources: %zu\n", offsetof(IDirect3DDevice9Vtbl, EvictManagedResources) / sizeof(void*));
    printf("GetDirect3D: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetDirect3D) / sizeof(void*));
    printf("GetDeviceCaps: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetDeviceCaps) / sizeof(void*));
    printf("GetDisplayMode: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetDisplayMode) / sizeof(void*));
    printf("GetCreationParameters: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetCreationParameters) / sizeof(void*));
    printf("SetCursorProperties: %zu\n", offsetof(IDirect3DDevice9Vtbl, SetCursorProperties) / sizeof(void*));
    printf("SetCursorPosition: %zu\n", offsetof(IDirect3DDevice9Vtbl, SetCursorPosition) / sizeof(void*));
    printf("ShowCursor: %zu\n", offsetof(IDirect3DDevice9Vtbl, ShowCursor) / sizeof(void*));
    printf("CreateAdditionalSwapChain: %zu\n", offsetof(IDirect3DDevice9Vtbl, CreateAdditionalSwapChain) / sizeof(void*));
    printf("GetSwapChain: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetSwapChain) / sizeof(void*));
    printf("GetNumberOfSwapChains: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetNumberOfSwapChains) / sizeof(void*));
    printf("Reset: %zu\n", offsetof(IDirect3DDevice9Vtbl, Reset) / sizeof(void*));
    printf("Present: %zu\n", offsetof(IDirect3DDevice9Vtbl, Present) / sizeof(void*));
    printf("GetBackBuffer: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetBackBuffer) / sizeof(void*));
    printf("GetRasterStatus: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetRasterStatus) / sizeof(void*));
    printf("SetDialogBoxMode: %zu\n", offsetof(IDirect3DDevice9Vtbl, SetDialogBoxMode) / sizeof(void*));
    printf("SetGammaRamp: %zu\n", offsetof(IDirect3DDevice9Vtbl, SetGammaRamp) / sizeof(void*));
    printf("GetGammaRamp: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetGammaRamp) / sizeof(void*));
    printf("CreateTexture: %zu\n", offsetof(IDirect3DDevice9Vtbl, CreateTexture) / sizeof(void*));
    printf("CreateVolumeTexture: %zu\n", offsetof(IDirect3DDevice9Vtbl, CreateVolumeTexture) / sizeof(void*));
    printf("CreateCubeTexture: %zu\n", offsetof(IDirect3DDevice9Vtbl, CreateCubeTexture) / sizeof(void*));
    printf("CreateVertexBuffer: %zu\n", offsetof(IDirect3DDevice9Vtbl, CreateVertexBuffer) / sizeof(void*));
    printf("CreateIndexBuffer: %zu\n", offsetof(IDirect3DDevice9Vtbl, CreateIndexBuffer) / sizeof(void*));
    printf("CreateRenderTarget: %zu\n", offsetof(IDirect3DDevice9Vtbl, CreateRenderTarget) / sizeof(void*));
    printf("CreateDepthStencilSurface: %zu\n", offsetof(IDirect3DDevice9Vtbl, CreateDepthStencilSurface) / sizeof(void*));
    printf("UpdateSurface: %zu\n", offsetof(IDirect3DDevice9Vtbl, UpdateSurface) / sizeof(void*));
    printf("UpdateTexture: %zu\n", offsetof(IDirect3DDevice9Vtbl, UpdateTexture) / sizeof(void*));
    printf("GetRenderTargetData: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetRenderTargetData) / sizeof(void*));
    printf("GetFrontBufferData: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetFrontBufferData) / sizeof(void*));
    printf("StretchRect: %zu\n", offsetof(IDirect3DDevice9Vtbl, StretchRect) / sizeof(void*));
    printf("ColorFill: %zu\n", offsetof(IDirect3DDevice9Vtbl, ColorFill) / sizeof(void*));
    printf("CreateOffscreenPlainSurface: %zu\n", offsetof(IDirect3DDevice9Vtbl, CreateOffscreenPlainSurface) / sizeof(void*));
    printf("SetRenderTarget: %zu\n", offsetof(IDirect3DDevice9Vtbl, SetRenderTarget) / sizeof(void*));
    printf("GetRenderTarget: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetRenderTarget) / sizeof(void*));
    printf("SetDepthStencilSurface: %zu\n", offsetof(IDirect3DDevice9Vtbl, SetDepthStencilSurface) / sizeof(void*));
    printf("GetDepthStencilSurface: %zu\n", offsetof(IDirect3DDevice9Vtbl, GetDepthStencilSurface) / sizeof(void*));
    printf("BeginScene: %zu\n", offsetof(IDirect3DDevice9Vtbl, BeginScene) / sizeof(void*));
    printf("EndScene: %zu\n", offsetof(IDirect3DDevice9Vtbl, EndScene) / sizeof(void*));
    printf("Clear: %zu\n", offsetof(IDirect3DDevice9Vtbl, Clear) / sizeof(void*));
    return 0;
}
