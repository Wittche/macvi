import re
with open("src/win32/d3d9.c", "r") as f:
    code = f.read()

def replace_func(m):
    return """static void d3d9_IDirect3D9_CreateDevice(EMU_CONTEXT* ctx) {
    uint64_t Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface;
    macwi_thunk_read_param_64(ctx, 2, &Adapter);
    macwi_thunk_read_param_64(ctx, 3, &DeviceType);
    macwi_thunk_read_param_64(ctx, 4, &hFocusWindow);
    macwi_thunk_read_param_64(ctx, 5, &BehaviorFlags);
    macwi_thunk_read_param_64(ctx, 6, &pPresentationParameters);
    macwi_thunk_read_param_64(ctx, 7, &ppReturnedDeviceInterface);
    
    D3D9_STUB_LOG("IDirect3D9::CreateDevice(hFocusWindow=0x%llX)", hFocusWindow);
    
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
    macwi_thunk_stdcall_return(ctx, 7);
}"""

code = re.sub(r'static void d3d9_IDirect3D9_CreateDevice.*?macwi_thunk_stdcall_return\(ctx, 7\);\n}', replace_func, code, flags=re.DOTALL)
with open("src/win32/d3d9.c", "w") as f:
    f.write(code)
