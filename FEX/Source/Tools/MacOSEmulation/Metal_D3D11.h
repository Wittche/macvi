#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stub for D3D11CreateDevice
uint64_t Metal_D3D11CreateDevice(
    void* pAdapter,
    uint32_t DriverType,
    void* Software,
    uint32_t Flags,
    void* pFeatureLevels,
    uint32_t FeatureLevels,
    uint32_t SDKVersion,
    void** ppDevice,
    void* pFeatureLevel,
    void** ppImmediateContext
);

// Stub for D3D11CreateDeviceAndSwapChain
uint64_t Metal_D3D11CreateDeviceAndSwapChain(
    void* pAdapter,
    uint32_t DriverType,
    void* Software,
    uint32_t Flags,
    void* pFeatureLevels,
    uint32_t FeatureLevels,
    uint32_t SDKVersion,
    void* pSwapChainDesc,
    void** ppSwapChain,
    void** ppDevice,
    void* pFeatureLevel,
    void** ppImmediateContext
);

#ifdef __cplusplus
}
#endif
