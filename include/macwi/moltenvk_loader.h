// ==============================================================================
// MacWI — MoltenVK Host Loader
// ==============================================================================
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque host pointer for vkGetInstanceProcAddr
typedef void* (*macwi_vkGetInstanceProcAddr_t)(void* instance, const char* pName);

// Initialize the host Vulkan library (MoltenVK)
// Returns true on success, false if Vulkan could not be loaded.
bool macwi_moltenvk_init(void);

// Shut down the host Vulkan library
void macwi_moltenvk_shutdown(void);

// Get the host's vkGetInstanceProcAddr pointer
macwi_vkGetInstanceProcAddr_t macwi_moltenvk_get_gpa(void);

#ifdef __cplusplus
}
#endif
