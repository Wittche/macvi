// ==============================================================================
// MacWI — MoltenVK Host Loader
// ==============================================================================
// SPDX-License-Identifier: MIT

#include <macwi/moltenvk_loader.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

static void* g_vulkan_handle = NULL;
static macwi_vkGetInstanceProcAddr_t g_vulkan_gpa = NULL;

bool macwi_moltenvk_init(void) {
    if (g_vulkan_handle) return true;

    const char* lib_names[] = {
        "libvulkan.1.dylib",
        "libvulkan.dylib",
        "libMoltenVK.dylib",
        "/usr/local/lib/libvulkan.dylib",
        "/opt/homebrew/lib/libvulkan.dylib"
    };

    for (int i = 0; i < sizeof(lib_names)/sizeof(lib_names[0]); i++) {
        g_vulkan_handle = dlopen(lib_names[i], RTLD_NOW | RTLD_LOCAL);
        if (g_vulkan_handle) {
            printf("[macwi:vulkan] Loaded Host Vulkan library: %s\n", lib_names[i]);
            break;
        }
    }

    if (!g_vulkan_handle) {
        fprintf(stderr, "[macwi:vulkan] Failed to load Host Vulkan library.\n");
        return false;
    }

    g_vulkan_gpa = (macwi_vkGetInstanceProcAddr_t)dlsym(g_vulkan_handle, "vkGetInstanceProcAddr");
    if (!g_vulkan_gpa) {
        fprintf(stderr, "[macwi:vulkan] Failed to find vkGetInstanceProcAddr in Host Vulkan.\n");
        dlclose(g_vulkan_handle);
        g_vulkan_handle = NULL;
        return false;
    }

    printf("[macwi:vulkan] Host vkGetInstanceProcAddr initialized successfully at %p\n", g_vulkan_gpa);
    return true;
}

void macwi_moltenvk_shutdown(void) {
    if (g_vulkan_handle) {
        dlclose(g_vulkan_handle);
        g_vulkan_handle = NULL;
        g_vulkan_gpa = NULL;
    }
}

macwi_vkGetInstanceProcAddr_t macwi_moltenvk_get_gpa(void) {
    return g_vulkan_gpa;
}
