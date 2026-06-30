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

uint64_t host_Direct3DCreate9(EMU_CONTEXT* ctx, uint32_t SDKVersion) {
    D3D9_STUB_LOG("Direct3DCreate9 called with SDKVersion=%u", SDKVersion);
    
    // We would create a COM object here and return a pointer to its vtable.
    // For now, we return a fake pointer (e.g., 0xD3D90000)
    return 0xD3D90000;
}

extern void fexi_register_d3d9(void);

void macwi_d3d9_register_apis(void) {
    fexi_register_d3d9();
}
