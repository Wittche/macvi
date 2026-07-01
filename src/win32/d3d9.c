/**
 * @file d3d9.c
 * @brief Win32 d3d9.dll stub implementations
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/types.h"
#include "macwi/emu.h"
#include "macwi/thunk.h"

#include <stdio.h>
#include <stdlib.h>

#define D3D9_STUB_LOG(fmt, ...) fprintf(stderr, "[macwi:d3d9] " fmt "\n", ##__VA_ARGS__)

extern void macwi_metal_init(void* cocoa_window);
extern void macwi_metal_clear(uint64_t color);

// Forward declarations for trampolines
static uint64_t g_d3d9_CreateDevice_tramp = 0;
static uint64_t g_d3d9_Release_tramp = 0;
static uint64_t g_d3ddevice9_Clear_tramp = 0;
static uint64_t g_d3ddevice9_BeginScene_tramp = 0;
static uint64_t g_d3ddevice9_EndScene_tramp = 0;
static uint64_t g_d3ddevice9_Present_tramp = 0;
static uint64_t g_d3ddevice9_Release_tramp = 0;

static void d3d9_IDirect3D9_Release(EMU_CONTEXT* ctx) {
    D3D9_STUB_LOG("IDirect3D9::Release()");
    macwi_emu_reg_write_32(ctx, 0, 0); // Refcount 0
    
}

static void d3d9_IDirect3DDevice9_Release(EMU_CONTEXT* ctx) {
    D3D9_STUB_LOG("IDirect3DDevice9::Release()");
    macwi_emu_reg_write_32(ctx, 0, 0); // Refcount 0
    
}

static void d3d9_IDirect3DDevice9_Clear(EMU_CONTEXT* ctx) {
    uint64_t count, pRects, Flags, Color, Z, Stencil;
    macwi_thunk_read_param_64(ctx, 1, &count);
    macwi_thunk_read_param_64(ctx, 2, &pRects);
    macwi_thunk_read_param_64(ctx, 3, &Flags);
    macwi_thunk_read_param_64(ctx, 4, &Color);
    macwi_thunk_read_param_64(ctx, 5, &Z);
    macwi_thunk_read_param_64(ctx, 6, &Stencil);
    
    D3D9_STUB_LOG("IDirect3DDevice9::Clear(Color=0x%08X)", Color);
    macwi_metal_clear(Color);
    
    macwi_emu_reg_write_32(ctx, 0, 0); // D3D_OK
    
}

static void d3d9_IDirect3DDevice9_BeginScene(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write_32(ctx, 0, 0); // D3D_OK
    
}

static void d3d9_IDirect3DDevice9_EndScene(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write_32(ctx, 0, 0); // D3D_OK
    
}

static void d3d9_IDirect3DDevice9_Present(EMU_CONTEXT* ctx) {
    D3D9_STUB_LOG("IDirect3DDevice9::Present()");
    macwi_emu_reg_write_32(ctx, 0, 0); // D3D_OK
    
}

static void d3d9_IDirect3D9_CreateDevice(EMU_CONTEXT* ctx) {
    uint64_t Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface;
    macwi_thunk_read_param_64(ctx, 1, &Adapter);
    macwi_thunk_read_param_64(ctx, 2, &DeviceType);
    macwi_thunk_read_param_64(ctx, 3, &hFocusWindow);
    macwi_thunk_read_param_64(ctx, 4, &BehaviorFlags);
    macwi_thunk_read_param_64(ctx, 5, &pPresentationParameters);
    macwi_thunk_read_param_64(ctx, 6, &ppReturnedDeviceInterface);
    
    D3D9_STUB_LOG("IDirect3D9::CreateDevice(hFocusWindow=0x%llX, ppRet=0x%llX)", hFocusWindow, ppReturnedDeviceInterface);
    
    // Get the cocoa window from the handle
    extern void* get_cocoa_window_from_hwnd(uint64_t hwnd); // We need to expose this or use handle table
    // For now, let's just initialize metal. 
    // In our simplified test, we just assume the first created window is the main window.
    extern void* g_main_cocoa_window; 
    
    if (g_main_cocoa_window) {
        macwi_metal_init(g_main_cocoa_window);
    }
    
    // Allocate IDirect3DDevice9 COM object in guest memory
    uint64_t device_base = 0; 
    macwi_emu_map_memory(ctx, 0, 0x1000, MACWI_PROT_ALL, &device_base);
    uint64_t vtable_base = device_base + 8;
    
    macwi_emu_write_memory(ctx, device_base, &vtable_base, 8);
    
    uint64_t val;
    val = (uint64_t)g_d3ddevice9_Release_tramp; macwi_emu_write_memory(ctx, vtable_base + 2*8, &val, 8); // Release
    val = (uint64_t)g_d3ddevice9_Present_tramp; macwi_emu_write_memory(ctx, vtable_base + 17*8, &val, 8); // Present
    val = (uint64_t)g_d3ddevice9_Clear_tramp; macwi_emu_write_memory(ctx, vtable_base + 43*8, &val, 8); // Clear
    val = (uint64_t)g_d3ddevice9_BeginScene_tramp; macwi_emu_write_memory(ctx, vtable_base + 41*8, &val, 8); // BeginScene
    val = (uint64_t)g_d3ddevice9_EndScene_tramp; macwi_emu_write_memory(ctx, vtable_base + 42*8, &val, 8); // EndScene
    
    macwi_emu_write_memory(ctx, ppReturnedDeviceInterface, &device_base, 8);
    
    macwi_emu_reg_write_64(ctx, 0, 0); // D3D_OK
    
}


uint64_t host_Direct3DCreate9(EMU_CONTEXT* ctx, uint64_t SDKVersion) {
    D3D9_STUB_LOG("Direct3DCreate9 called with SDKVersion=%llu", SDKVersion);
    
    // Map memory for IDirect3D9 COM object in guest memory
    uint64_t obj_base = 0;
    macwi_emu_map_memory(ctx, 0, 0x1000, MACWI_PROT_ALL, &obj_base);
    uint64_t vtable_base = obj_base + 8; // 8 bytes for vtable pointer in 64-bit
    
    // Set vtable pointer
    macwi_emu_write_memory(ctx, obj_base, &vtable_base, 8);
    
    // Populate VTable (partial)
    uint64_t val;
    val = (uint64_t)g_d3d9_Release_tramp; macwi_emu_write_memory(ctx, vtable_base + 2*8, &val, 8); // Release
    val = (uint64_t)g_d3d9_CreateDevice_tramp; macwi_emu_write_memory(ctx, vtable_base + 16*8, &val, 8); // CreateDevice
    
    return obj_base;
}

extern void fexi_register_d3d9(void);

void macwi_d3d9_register_apis(void) {
    fexi_register_d3d9();
    
    // Register COM interface methods as manual thunks
    macwi_thunk_register_api("d3d9_internal", "IDirect3D9_Release", d3d9_IDirect3D9_Release, 1);
    macwi_thunk_register_api("d3d9_internal", "IDirect3D9_CreateDevice", d3d9_IDirect3D9_CreateDevice, 7);
    
    macwi_thunk_register_api("d3d9_internal", "IDirect3DDevice9_Release", d3d9_IDirect3DDevice9_Release, 1);
    macwi_thunk_register_api("d3d9_internal", "IDirect3DDevice9_Clear", d3d9_IDirect3DDevice9_Clear, 7);
    macwi_thunk_register_api("d3d9_internal", "IDirect3DDevice9_BeginScene", d3d9_IDirect3DDevice9_BeginScene, 1);
    macwi_thunk_register_api("d3d9_internal", "IDirect3DDevice9_EndScene", d3d9_IDirect3DDevice9_EndScene, 1);
    macwi_thunk_register_api("d3d9_internal", "IDirect3DDevice9_Present", d3d9_IDirect3DDevice9_Present, 5);
}

void macwi_d3d9_init_trampolines(EMU_CONTEXT* ctx) {
    g_d3d9_Release_tramp = macwi_thunk_get_trampoline(ctx, "d3d9_internal", "IDirect3D9_Release");
    g_d3d9_CreateDevice_tramp = macwi_thunk_get_trampoline(ctx, "d3d9_internal", "IDirect3D9_CreateDevice");
    
    g_d3ddevice9_Release_tramp = macwi_thunk_get_trampoline(ctx, "d3d9_internal", "IDirect3DDevice9_Release");
    g_d3ddevice9_Clear_tramp = macwi_thunk_get_trampoline(ctx, "d3d9_internal", "IDirect3DDevice9_Clear");
    g_d3ddevice9_BeginScene_tramp = macwi_thunk_get_trampoline(ctx, "d3d9_internal", "IDirect3DDevice9_BeginScene");
    g_d3ddevice9_EndScene_tramp = macwi_thunk_get_trampoline(ctx, "d3d9_internal", "IDirect3DDevice9_EndScene");
    g_d3ddevice9_Present_tramp = macwi_thunk_get_trampoline(ctx, "d3d9_internal", "IDirect3DDevice9_Present");
}
