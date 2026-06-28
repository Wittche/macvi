#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>
#include <iostream>
#include "Metal_D3D11.h"

extern "C" {

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
) {
    fprintf(stderr, "[Metal_D3D11] D3D11CreateDevice called!\n");
    // TODO: Create a Metal device wrapper and return S_OK (0)
    // For now, just return E_NOTIMPL (0x80004001) or S_OK (0) to pretend it worked.
    return 0; // S_OK
}

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
) {
    fprintf(stderr, "[Metal_D3D11] D3D11CreateDeviceAndSwapChain called!\n");
    // TODO: Create a Metal device and CAMetalLayer swapchain.
    return 0; // S_OK
}

}
