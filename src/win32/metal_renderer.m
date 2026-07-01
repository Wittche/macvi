/**
 * @file metal_renderer.m
 * @brief Metal renderer for MacWI to support Direct3D 9 hardware acceleration
 *
 * SPDX-License-Identifier: MIT
 */

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <stdint.h>
#include <stdio.h>

static id<MTLDevice> g_mtlDevice = nil;
static id<MTLCommandQueue> g_commandQueue = nil;
static CAMetalLayer* g_metalLayer = nil;

void macwi_metal_init(void* cocoa_window) {
    if (g_mtlDevice != nil) {
        return; // Already initialized
    }
    
    dispatch_sync(dispatch_get_main_queue(), ^{
        printf("[macwi:metal] Initializing Metal Device...\n");
        g_mtlDevice = MTLCreateSystemDefaultDevice();
        if (!g_mtlDevice) {
            fprintf(stderr, "[macwi:metal] FATAL: Metal is not supported on this device.\n");
            return;
        }
        
        g_commandQueue = [g_mtlDevice newCommandQueue];
        
        NSWindow* window = (__bridge NSWindow*)cocoa_window;
        NSView* contentView = [window contentView];
        
        [contentView setWantsLayer:YES];
        
        g_metalLayer = [CAMetalLayer layer];
        g_metalLayer.device = g_mtlDevice;
        g_metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        g_metalLayer.framebufferOnly = YES;
        g_metalLayer.frame = contentView.bounds;
        
        [contentView setLayer:g_metalLayer];
        
        printf("[macwi:metal] Metal initialized successfully. Device: %s\n", [[g_mtlDevice name] UTF8String]);
    });
}

void macwi_metal_clear(uint32_t color) {
    if (!g_metalLayer || !g_commandQueue) return;
    
    dispatch_sync(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            id<CAMetalDrawable> drawable = [g_metalLayer nextDrawable];
            if (!drawable) return;
            
            MTLRenderPassDescriptor* passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
            passDescriptor.colorAttachments[0].texture = drawable.texture;
            passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
            passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
            
            // Convert ARGB to RGBA floating point
            float a = ((color >> 24) & 0xFF) / 255.0f;
            float r = ((color >> 16) & 0xFF) / 255.0f;
            float g = ((color >> 8)  & 0xFF) / 255.0f;
            float b = ((color)       & 0xFF) / 255.0f;
            
            passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(r, g, b, a);
            
            id<MTLCommandBuffer> commandBuffer = [g_commandQueue commandBuffer];
            id<MTLRenderCommandEncoder> commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
            [commandEncoder endEncoding];
            
            [commandBuffer presentDrawable:drawable];
            [commandBuffer commit];
        }
    });
}
