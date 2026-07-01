import re
with open("src/win32/d3d9.c", "r") as f:
    code = f.read()

def replace_func(m):
    return """uint64_t host_Direct3DCreate9(EMU_CONTEXT* ctx, uint64_t SDKVersion) {
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
}"""

code = re.sub(r'uint64_t host_Direct3DCreate9.*?return obj_base;\n}', replace_func, code, flags=re.DOTALL)
with open("src/win32/d3d9.c", "w") as f:
    f.write(code)
