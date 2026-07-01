// ==============================================================================
// MacWI — vulkan-1.dll Stub
// ==============================================================================
// SPDX-License-Identifier: MIT

#include <macwi/emu.h>
#include <macwi/thunk.h>
#include <macwi/moltenvk_loader.h>
#include <stdio.h>
#include <string.h>

// Include the auto-generated thunks and the registration function
#include "vulkan_thunks_gen.inc"

static void win32_vkGetInstanceProcAddr(EMU_CONTEXT* ctx) {
    uint64_t instance;
    uint64_t pName;
    macwi_thunk_read_param_64(ctx, 0, &instance);
    macwi_thunk_read_param_64(ctx, 1, &pName);

    char name_buf[256] = {0};
    if (pName) {
        macwi_emu_read_memory(ctx, pName, name_buf, sizeof(name_buf)-1);
    }

    fprintf(stderr, "[macwi:vulkan] vkGetInstanceProcAddr(0x%llX, \"%s\")\n", instance, name_buf);

    if (strcmp(name_buf, "vkGetInstanceProcAddr") == 0) {
        // Return a pointer to ourselves!
        uint64_t tramp_va = macwi_thunk_get_trampoline(ctx, "vulkan-1.dll", "vkGetInstanceProcAddr");
        macwi_emu_reg_write_64(ctx, 0, tramp_va);
        macwi_thunk_stdcall_return(ctx, 2);
        return;
    }

    uint64_t thunk_addr = macwi_thunk_get_trampoline(ctx, "vulkan-1.dll", name_buf);
    if (thunk_addr) {
        fprintf(stderr, "[macwi:vulkan] Generated thunk for %s at 0x%llX\n", name_buf, thunk_addr);
        macwi_emu_reg_write_64(ctx, 0, thunk_addr);
    } else {
        fprintf(stderr, "[macwi:vulkan] Unsupported Vulkan function: %s\n", name_buf);
        macwi_emu_reg_write_64(ctx, 0, 0);
    }
    
    macwi_thunk_stdcall_return(ctx, 2);
}

void macwi_vulkan_register_apis(EMU_CONTEXT* ctx) {
    // Initialize MoltenVK host side
    macwi_moltenvk_init();

    // Register all generated thunks (this populates macwi_thunk system so they can be looked up)
    macwi_vulkan_register_all_thunks(ctx);

    // Override the generic generated vkGetInstanceProcAddr with our custom logic
    macwi_thunk_register_api("vulkan-1.dll", "vkGetInstanceProcAddr", win32_vkGetInstanceProcAddr, 2);
}
