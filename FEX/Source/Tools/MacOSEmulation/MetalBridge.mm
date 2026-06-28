#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>
#include <iostream>
#include <dispatch/dispatch.h>
#include <map>
#import <CoreText/CoreText.h>
#include <mach/mach_time.h>
#include "MetalBridge.h"

extern "C" void* GetMacDrvWindow(uint64_t hwnd);
extern "C" void macdrv_window_set_color_image(void* w, CGImageRef image, CGRect rect, CGRect dirty);

static void RunOnMainThreadSync(void (^block)(void)) {
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}

static id<MTLDevice> g_device = nil;
static id<MTLCommandQueue> g_commandQueue = nil;
static NSWindow* g_testWindow = nil;

extern "C" {

void EnsureMetalReady() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        if (!g_device) {
            g_device = MTLCreateSystemDefaultDevice();
            g_commandQueue = [g_device newCommandQueue];
            fprintf(stderr, "[MetalBridge] Device initialized: %s\n", [g_device.name UTF8String]);
        }
    });
}

struct HdcContext {
    uint64_t hwnd;
    CGContextRef cgContext;
    int width;
    int height;
    void* buffer;
    uint32_t textColor;
    uint32_t bkColor;
    int bkMode;
};

// Forward declarations for GDI methods
void* Metal_GetDC(uint64_t hwnd);
void Metal_FillRect(void* hdc, void* rect, uint32_t color);
void Metal_ExtTextOutW(void* hdc, int x, int y, uint32_t options, void* lprect, const uint16_t* text, int len, const int* dx) {
    struct HdcContext* ctx = (struct HdcContext*)hdc;
    if (!ctx) return;
    
    NSString* str = [[NSString alloc] initWithCharacters:text length:len];
    
    // Windows COLORREF is 0x00bbggrr
    float tr = (ctx->textColor & 0xFF) / 255.0f;
    float tg = ((ctx->textColor >> 8) & 0xFF) / 255.0f;
    float tb = ((ctx->textColor >> 16) & 0xFF) / 255.0f;
    
    NSColor* nsTextColor = [NSColor colorWithDeviceRed:tr green:tg blue:tb alpha:1.0];
    
    NSDictionary* attrs = @{
        NSFontAttributeName: [NSFont systemFontOfSize:14.0],
        NSForegroundColorAttributeName: nsTextColor
    };
    
    NSAttributedString* attrStr = [[NSAttributedString alloc] initWithString:str attributes:attrs];
    CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)attrStr);
    
    CGContextSaveGState(ctx->cgContext);
    
    // CoreGraphics origin is bottom-left, Windows is top-left
    CGContextTranslateCTM(ctx->cgContext, 0, ctx->height);
    CGContextScaleCTM(ctx->cgContext, 1.0, -1.0);
    
    // Background filling
    if (ctx->bkMode == 2) { // OPAQUE
        float br = (ctx->bkColor & 0xFF) / 255.0f;
        float bg = ((ctx->bkColor >> 8) & 0xFF) / 255.0f;
        float bb = ((ctx->bkColor >> 16) & 0xFF) / 255.0f;
        
        CGSize textSize = [str sizeWithAttributes:attrs];
        CGContextSetRGBFillColor(ctx->cgContext, br, bg, bb, 1.0);
        CGContextFillRect(ctx->cgContext, CGRectMake(x, y, textSize.width, textSize.height));
    }
    
    CGContextSetTextPosition(ctx->cgContext, x, y + 12); // Adjust baseline
    CTLineDraw(line, ctx->cgContext);
    
    CGContextRestoreGState(ctx->cgContext);
    CFRelease(line);
}

void Metal_FillRect(void* hdc, void* rect, uint32_t color) {
    struct HdcContext* ctx = (struct HdcContext*)hdc;
    if (!ctx || !rect) return;
    
    int* r = (int*)rect;
    int left = r[0], top = r[1], right = r[2], bottom = r[3];
    
    float cr = (color & 0xFF) / 255.0f;
    float cg = ((color >> 8) & 0xFF) / 255.0f;
    float cb = ((color >> 16) & 0xFF) / 255.0f;
    
    CGContextSaveGState(ctx->cgContext);
    CGContextTranslateCTM(ctx->cgContext, 0, ctx->height);
    CGContextScaleCTM(ctx->cgContext, 1.0, -1.0);
    
    CGContextSetRGBFillColor(ctx->cgContext, cr, cg, cb, 1.0);
    CGContextFillRect(ctx->cgContext, CGRectMake(left, top, right - left, bottom - top));
    
    CGContextRestoreGState(ctx->cgContext);
}

void Metal_SetTextColor(void* hdc, uint32_t color) {
    struct HdcContext* ctx = (struct HdcContext*)hdc;
    if (ctx) ctx->textColor = color;
}

void Metal_SetBkColor(void* hdc, uint32_t color) {
    struct HdcContext* ctx = (struct HdcContext*)hdc;
    if (ctx) ctx->bkColor = color;
}

void Metal_SetBkMode(void* hdc, int mode) {
    struct HdcContext* ctx = (struct HdcContext*)hdc;
    if (ctx) ctx->bkMode = mode;
}

void Metal_PresentDC(uint64_t hwnd, void* hdc);
void Metal_ReleaseDC(uint64_t hwnd, void* hdc);

void* Metal_CreateDevice() {
    EnsureMetalReady();
    return (__bridge void*)g_device;
}

void* Metal_CreateLayer(uint64_t window_handle) {
    EnsureMetalReady();
    if (!window_handle) return nullptr;

    __block CAMetalLayer* layer = nil;
    RunOnMainThreadSync(^{
        NSWindow* window = (__bridge NSWindow*)(void*)window_handle;
        layer = [CAMetalLayer layer];
        layer.device = g_device;
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        layer.framebufferOnly = NO; // MUST be NO for CPU replaceRegion
        layer.frame = window.contentView.bounds;
        
        // Ensure retina scaling
        layer.contentsScale = window.backingScaleFactor;
        layer.drawableSize = [window.contentView convertSizeToBacking:window.contentView.bounds.size];
        
        window.contentView.layer = layer;
        window.contentView.wantsLayer = YES;
        fprintf(stderr, "[MetalBridge] Layer attached to window. Size: %.0fx%.0f\n", layer.frame.size.width, layer.frame.size.height);
    });
    return (__bridge_retained void*)layer;
}

void Metal_ClearAndPresent(void* layer_ptr, float r, float g, float b, float a) {
    if (!layer_ptr) {
        static bool warned = false;
        if (!warned) { fprintf(stderr, "[MetalBridge] ERROR: ClearAndPresent called with NULL layer!\n"); warned = true; }
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        CAMetalLayer* layer = (__bridge CAMetalLayer*)layer_ptr;
        id<CAMetalDrawable> drawable = [layer nextDrawable];
        if (!drawable) {
            return;
        }

        MTLRenderPassDescriptor* passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        passDescriptor.colorAttachments[0].texture = drawable.texture;
        passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
        passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 1.0, 1.0); // FORCED BLUE

        id<MTLCommandBuffer> commandBuffer = [g_commandQueue commandBuffer];
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithPassDescriptor:passDescriptor];
        [renderEncoder endEncoding];

        [commandBuffer presentDrawable:drawable];
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> cb) {
            if (cb.status == MTLCommandBufferStatusError) {
                fprintf(stderr, "[MetalBridge] GPU Error: %s\n", [[cb.error localizedDescription] UTF8String]);
            }
        }];
        [commandBuffer commit];

        static int count = 0;
        if (count++ % 100 == 0) fprintf(stderr, "[MetalBridge] Frame %d committed to GPU\n", count);
    });
}


void* Metal_CreateSemaphore() {
    return (void*)dispatch_semaphore_create(0);
}

void Metal_WaitSemaphore(void* sem) {
    if (sem) dispatch_semaphore_wait((dispatch_semaphore_t)sem, DISPATCH_TIME_FOREVER);
}

void Metal_SignalSemaphore(void* sem) {
    if (sem) dispatch_semaphore_signal((dispatch_semaphore_t)sem);
}

void* Metal_CreateBuffer(void* device_ptr, const void* data, uint32_t size) {
    if (!device_ptr) return nullptr;
    id<MTLDevice> dev = (__bridge id<MTLDevice>)device_ptr;
    id<MTLBuffer> buffer = nil;
    if (data) {
        buffer = [dev newBufferWithBytes:data length:size options:MTLResourceStorageModeShared];
    } else {
        buffer = [dev newBufferWithLength:size options:MTLResourceStorageModeShared];
    }
    fprintf(stderr, "[MetalBridge] Buffer created: size %u\n", size);
    return (__bridge_retained void*)buffer;
}

void* Metal_CreateVertexShader(void* device_ptr, const void* source, uint32_t size) {
    fprintf(stderr, "[MetalBridge] CreateVertexShader stubbed\n");
    // Full implementation requires DXBC to MSL translation (e.g. using SPIRV-Cross or similar)
    // For now we just return a dummy proxy object
    return (void*)0x12345678; 
}

void* Metal_CreatePixelShader(void* device_ptr, const void* source, uint32_t size) {
    fprintf(stderr, "[MetalBridge] CreatePixelShader stubbed\n");
    return (void*)0x87654321;
}

void Metal_SetVertexBuffer(void* context_ptr, void* buffer_ptr, uint32_t stride, uint32_t offset) {
    // We would need the current render encoder
    fprintf(stderr, "[MetalBridge] SetVertexBuffer (stride %u, offset %u)\n", stride, offset);
}

void Metal_SetConstantBuffer(void* context_ptr, uint32_t stage, uint32_t slot, void* buffer_ptr) {
    fprintf(stderr, "[MetalBridge] SetConstantBuffer (stage %u, slot %u)\n", stage, slot);
}

void Metal_SetTexture(void* context_ptr, uint32_t stage, uint32_t slot, void* texture_ptr) {
    fprintf(stderr, "[MetalBridge] SetTexture (stage %u, slot %u)\n", stage, slot);
}

void Metal_Draw(void* context_ptr, uint32_t vertexCount, uint32_t startVertexLocation) {
    fprintf(stderr, "[MetalBridge] Draw stubbed (count %u, start %u)\n", vertexCount, startVertexLocation);
}

uint64_t Metal_CreateWindow(uint64_t hwndParent, uint32_t style, const char* title, int x, int y, int w, int h) {
    __block id resultObj = nil;
    NSString* nsTitle = [NSString stringWithUTF8String:title ? title : "FEX Window"];
    
    // Sanity check for dimensions
    if (w <= 0) w = 800;
    if (h <= 0) h = 600;

    fprintf(stderr, "[MetalBridge] Creating window '%s' at (%d, %d) size %dx%d (Parent: 0x%llx, Style: 0x%x)\n", title ? title : "FEX", x, y, w, h, hwndParent, style);

    RunOnMainThreadSync(^{
        if ((style & 0x40000000) != 0 && hwndParent != 0) { // WS_CHILD
            id parent = (__bridge id)(void*)hwndParent;
            NSView* parentView = nil;
            if ([parent isKindOfClass:[NSWindow class]]) {
                parentView = ((NSWindow*)parent).contentView;
            } else if ([parent isKindOfClass:[NSView class]]) {
                parentView = (NSView*)parent;
            }
            
            if (parentView) {
                CGFloat parentH = parentView.bounds.size.height;
                NSRect rect = NSMakeRect(x, parentH - y - h, w, h);
                NSView* view = [[NSView alloc] initWithFrame:rect];
                [view setWantsLayer:YES];
                
                EnsureMetalReady();
                CAMetalLayer* layer = [CAMetalLayer layer];
                layer.device = g_device;
                layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
                layer.framebufferOnly = NO;
                layer.frame = view.bounds;
                layer.contentsScale = parentView.window.backingScaleFactor;
                layer.drawableSize = [view convertSizeToBacking:view.bounds.size];
                
                view.layer = layer;
                [parentView addSubview:view];
                resultObj = view;
                fprintf(stderr, "[MetalBridge] Created CHILD VIEW at (%d, %d) size %dx%d\n", x, y, w, h);
            }
        } else {
            NSRect rect = NSMakeRect(x, y, w, h);
            NSWindow* window = [[NSWindow alloc] initWithContentRect:rect
                                                 styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
            [window setTitle:nsTitle];
            [window setReleasedWhenClosed:NO];
            [window makeKeyAndOrderFront:nil];
            [window orderFrontRegardless];
            [NSApp activateIgnoringOtherApps:YES];
            
            EnsureMetalReady();
            CAMetalLayer* layer = [CAMetalLayer layer];
            layer.device = g_device;
            layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            layer.framebufferOnly = NO; // MUST be NO for CPU replaceRegion
            layer.frame = window.contentView.bounds;
            layer.contentsScale = window.backingScaleFactor;
            layer.drawableSize = [window.contentView convertSizeToBacking:window.contentView.bounds.size];
            
            window.contentView.layer = layer;
            window.contentView.wantsLayer = YES;
            g_testWindow = window;
            
            resultObj = window;
            fprintf(stderr, "[MetalBridge] Created TOP LEVEL window.\n");
        }
    });
    return (uint64_t)(__bridge_retained void*)resultObj;
}

int Metal_PollEvents() {
    __block int count = 0;
    RunOnMainThreadSync(^{
        while (true) {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                 untilDate:[NSDate distantPast]
                                                    inMode:NSDefaultRunLoopMode
                                                   dequeue:YES];
            if (!event) break;
            [NSApp sendEvent:event];
            count++;
        }
    });
    return count;
}

extern "C" void PumpMacEvents() {
    RunOnMainThreadSync(^{
        NSEvent* event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                             untilDate:[NSDate distantPast]
                                                inMode:NSDefaultRunLoopMode
                                               dequeue:YES])) {
            [NSApp sendEvent:event];
        }
    });
}

static BOOL g_WinecfgWindowOpen = NO;

bool FEX_IsWinecfgWindowOpen() {
    return g_WinecfgWindowOpen;
}

@interface WinecfgUIHandler : NSObject
- (void)onVersionChanged:(id)sender;
- (void)onAudioChanged:(id)sender;
- (void)onDriveChanged:(id)sender;
- (void)onShellFolderChanged:(id)sender;
@end

@implementation WinecfgUIHandler
- (void)onVersionChanged:(id)sender {
    NSPopUpButton* popUp = (NSPopUpButton*)sender;
    NSString* version = [popUp titleOfSelectedItem];
    fprintf(stderr, "[FEXMacOS-UI] Version changed to: %s\n", [version UTF8String]);
    
    extern uint32_t FEX_RegistryOpenKey(uint64_t RootKey, const char* SubKey, uint64_t* OutHandle);
    extern uint32_t FEX_RegistrySetValue(uint64_t KeyHandle, const char* ValueName, uint32_t Type, const void* Data, uint32_t DataSize);
    extern uint32_t FEX_RegistryCloseKey(uint64_t KeyHandle);
    
    uint64_t hKey = 0;
    if (FEX_RegistryOpenKey(0x80000002, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", &hKey) == 0) {
        std::string prod_name = std::string([version UTF8String]) + " Pro";
        FEX_RegistrySetValue(hKey, "ProductName", 1, prod_name.c_str(), prod_name.size() + 1);
        
        std::string build = "19045";
        if ([version isEqualToString:@"Windows 11"]) build = "22000";
        else if ([version isEqualToString:@"Windows 7"]) build = "7601";
        FEX_RegistrySetValue(hKey, "CurrentBuild", 1, build.c_str(), build.size() + 1);
        
        FEX_RegistryCloseKey(hKey);
    }
}

- (void)onAudioChanged:(id)sender {
    NSPopUpButton* popUp = (NSPopUpButton*)sender;
    NSString* audio = [popUp titleOfSelectedItem];
    fprintf(stderr, "[FEXMacOS-UI] Audio driver changed to: %s\n", [audio UTF8String]);
    
    extern uint32_t FEX_RegistryOpenKey(uint64_t RootKey, const char* SubKey, uint64_t* OutHandle);
    extern uint32_t FEX_RegistrySetValue(uint64_t KeyHandle, const char* ValueName, uint32_t Type, const void* Data, uint32_t DataSize);
    extern uint32_t FEX_RegistryCloseKey(uint64_t KeyHandle);
    
    uint64_t hKey = 0;
    if (FEX_RegistryOpenKey(0x80000001, "Software\\Wine\\Drivers", &hKey) == 0) {
        std::string audio_str = [audio UTF8String];
        FEX_RegistrySetValue(hKey, "Audio", 1, audio_str.c_str(), audio_str.size() + 1);
        FEX_RegistryCloseKey(hKey);
    }
}

- (void)onDriveChanged:(id)sender {
    NSTextField* field = (NSTextField*)sender;
    // On macOS, editing fields can trigger action.
    NSString* path = [field stringValue];
    NSString* driveLetter = (NSString*)[field identifier];
    fprintf(stderr, "[FEXMacOS-UI] Drive %s path changed to: %s\n", [driveLetter UTF8String], [path UTF8String]);
    
    extern uint32_t FEX_RegistryOpenKey(uint64_t RootKey, const char* SubKey, uint64_t* OutHandle);
    extern uint32_t FEX_RegistrySetValue(uint64_t KeyHandle, const char* ValueName, uint32_t Type, const void* Data, uint32_t DataSize);
    extern uint32_t FEX_RegistryCloseKey(uint64_t KeyHandle);
    
    uint64_t hKey = 0;
    if (FEX_RegistryOpenKey(0x80000001, "Software\\Wine\\Drives", &hKey) == 0) {
        std::string path_str = [path UTF8String];
        std::string drive_str = [driveLetter UTF8String];
        FEX_RegistrySetValue(hKey, drive_str.c_str(), 1, path_str.c_str(), path_str.size() + 1);
        FEX_RegistryCloseKey(hKey);
    }
}

- (void)onShellFolderChanged:(id)sender {
    NSTextField* field = (NSTextField*)sender;
    NSString* path = [field stringValue];
    NSString* folderType = (NSString*)[field identifier];
    fprintf(stderr, "[FEXMacOS-UI] Shell folder %s changed to: %s\n", [folderType UTF8String], [path UTF8String]);
    
    extern uint32_t FEX_RegistryOpenKey(uint64_t RootKey, const char* SubKey, uint64_t* OutHandle);
    extern uint32_t FEX_RegistrySetValue(uint64_t KeyHandle, const char* ValueName, uint32_t Type, const void* Data, uint32_t DataSize);
    extern uint32_t FEX_RegistryCloseKey(uint64_t KeyHandle);
    
    uint64_t hKey = 0;
    if (FEX_RegistryOpenKey(0x80000001, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", &hKey) == 0) {
        std::string path_str = [path UTF8String];
        std::string folder_str = [folderType UTF8String];
        FEX_RegistrySetValue(hKey, folder_str.c_str(), 1, path_str.c_str(), path_str.size() + 1);
        FEX_RegistryCloseKey(hKey);
    }
}
@end

static WinecfgUIHandler* g_UIHandler = nil;

void FEX_ShowWinecfgWindow() {
    RunOnMainThreadSync(^{
        if (!g_UIHandler) {
            g_UIHandler = [[WinecfgUIHandler alloc] init];
        }

        NSRect frame = NSMakeRect(0, 0, 640, 480);
        NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        [window setTitle:@"Wine Configuration (Apple Silicon JIT Bridge)"];
        [window setReleasedWhenClosed:NO];
        [window center];
        
        NSTabView* tabView = [[NSTabView alloc] initWithFrame:NSMakeRect(10, 10, 620, 430)];
        
        NSArray* tabs = @[@"Applications", @"Libraries", @"Graphics", @"Desktop Integration", @"Drives", @"Audio", @"About"];
        for (NSString* tabTitle in tabs) {
            NSTabViewItem* item = [[NSTabViewItem alloc] initWithIdentifier:tabTitle];
            [item setLabel:tabTitle];
            
            NSView* container = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 620, 400)];
            
            NSTextField* titleLabel = [NSTextField labelWithString:[NSString stringWithFormat:@"%@ Settings", tabTitle]];
            [titleLabel setFrame:NSMakeRect(20, 350, 580, 30)];
            [titleLabel setFont:[NSFont boldSystemFontOfSize:18]];
            [titleLabel setTextColor:[NSColor labelColor]];
            [container addSubview:titleLabel];
            
            if ([tabTitle isEqualToString:@"Applications"]) {
                NSTextField* desc = [NSTextField labelWithString:@"Windows Version:"];
                [desc setFrame:NSMakeRect(20, 300, 150, 20)];
                [container addSubview:desc];
                
                NSPopUpButton* popUp = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(180, 298, 150, 25) pullsDown:NO];
                [popUp addItemsWithTitles:@[@"Windows 10", @"Windows 11", @"Windows 7"]];
                
                // Read current value to set default selection
                extern uint32_t FEX_RegistryOpenKey(uint64_t RootKey, const char* SubKey, uint64_t* OutHandle);
                extern uint32_t FEX_RegistryQueryValue(uint64_t KeyHandle, const char* ValueName, uint32_t* OutType, void* Data, uint32_t* DataSize);
                extern uint32_t FEX_RegistryCloseKey(uint64_t KeyHandle);
                uint64_t hKey = 0;
                char current_ver[128] = {0};
                if (FEX_RegistryOpenKey(0x80000002, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", &hKey) == 0) {
                    uint32_t type = 0;
                    uint32_t size = sizeof(current_ver);
                    if (FEX_RegistryQueryValue(hKey, "ProductName", &type, current_ver, &size) == 0) {
                        NSString* nsVer = [NSString stringWithUTF8String:current_ver];
                        if ([nsVer containsString:@"Windows 11"]) [popUp selectItemWithTitle:@"Windows 11"];
                        else if ([nsVer containsString:@"Windows 7"]) [popUp selectItemWithTitle:@"Windows 7"];
                        else [popUp selectItemWithTitle:@"Windows 10"];
                    }
                    FEX_RegistryCloseKey(hKey);
                }
                
                [popUp setTarget:g_UIHandler];
                [popUp setAction:@selector(onVersionChanged:)];
                [container addSubview:popUp];
            } else if ([tabTitle isEqualToString:@"Libraries"]) {
                NSTextField* desc = [NSTextField labelWithString:@"New override for library:"];
                [desc setFrame:NSMakeRect(20, 300, 200, 20)];
                [container addSubview:desc];
                
                NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(220, 298, 150, 22)];
                [input setStringValue:@"d3d11"];
                [container addSubview:input];
            } else if ([tabTitle isEqualToString:@"Graphics"]) {
                NSButton* check = [NSButton checkboxWithTitle:@"Allow the window manager to decorate the windows" target:nil action:nil];
                [check setFrame:NSMakeRect(20, 300, 400, 20)];
                [check setState:NSControlStateValueOn];
                [container addSubview:check];
                
                NSButton* check2 = [NSButton checkboxWithTitle:@"Allow the window manager to control the windows" target:nil action:nil];
                [check2 setFrame:NSMakeRect(20, 270, 400, 20)];
                [check2 setState:NSControlStateValueOn];
                [container addSubview:check2];
            } else if ([tabTitle isEqualToString:@"About"]) {
                NSTextField* version = [NSTextField labelWithString:@"Wine Version 11.8 (FEX JIT Emulated)"];
                [version setFrame:NSMakeRect(20, 290, 580, 20)];
                [version setFont:[NSFont systemFontOfSize:14]];
                [container addSubview:version];
                
                NSTextField* copyright = [NSTextField labelWithString:@"Copyright (c) 1993-2026 Wine Developers."];
                [copyright setFrame:NSMakeRect(20, 260, 580, 20)];
                [container addSubview:copyright];
            } else if ([tabTitle isEqualToString:@"Drives"]) {
                NSTextField* desc = [NSTextField labelWithString:@"Drive Mappings:"];
                [desc setFrame:NSMakeRect(20, 310, 150, 20)];
                [desc setFont:[NSFont boldSystemFontOfSize:13]];
                [container addSubview:desc];
                
                NSTextField* labelC = [NSTextField labelWithString:@"C: (System)"];
                [labelC setFrame:NSMakeRect(20, 270, 100, 20)];
                [container addSubview:labelC];
                
                NSTextField* inputC = [[NSTextField alloc] initWithFrame:NSMakeRect(130, 268, 400, 22)];
                [inputC setIdentifier:@"c:"];
                [inputC setTarget:g_UIHandler];
                [inputC setAction:@selector(onDriveChanged:)];
                [container addSubview:inputC];
                
                NSTextField* labelD = [NSTextField labelWithString:@"D: (Mount)"];
                [labelD setFrame:NSMakeRect(20, 230, 100, 20)];
                [container addSubview:labelD];
                
                NSTextField* inputD = [[NSTextField alloc] initWithFrame:NSMakeRect(130, 228, 400, 22)];
                [inputD setIdentifier:@"d:"];
                [inputD setTarget:g_UIHandler];
                [inputD setAction:@selector(onDriveChanged:)];
                [container addSubview:inputD];
                
                extern uint32_t FEX_RegistryOpenKey(uint64_t RootKey, const char* SubKey, uint64_t* OutHandle);
                extern uint32_t FEX_RegistryQueryValue(uint64_t KeyHandle, const char* ValueName, uint32_t* OutType, void* Data, uint32_t* DataSize);
                extern uint32_t FEX_RegistryCloseKey(uint64_t KeyHandle);
                uint64_t hKey = 0;
                if (FEX_RegistryOpenKey(0x80000001, "Software\\Wine\\Drives", &hKey) == 0) {
                    char path[256];
                    uint32_t type = 0, size = sizeof(path);
                    if (FEX_RegistryQueryValue(hKey, "c:", &type, path, &size) == 0) {
                        [inputC setStringValue:[NSString stringWithUTF8String:path]];
                    }
                    size = sizeof(path);
                    if (FEX_RegistryQueryValue(hKey, "d:", &type, path, &size) == 0) {
                        [inputD setStringValue:[NSString stringWithUTF8String:path]];
                    }
                    FEX_RegistryCloseKey(hKey);
                }
                
            } else if ([tabTitle isEqualToString:@"Audio"]) {
                NSTextField* desc = [NSTextField labelWithString:@"Audio Driver:"];
                [desc setFrame:NSMakeRect(20, 300, 150, 20)];
                [container addSubview:desc];
                
                NSPopUpButton* popUp = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(180, 298, 150, 25) pullsDown:NO];
                [popUp addItemsWithTitles:@[@"coreaudio", @"Null Driver"]];
                [popUp setTarget:g_UIHandler];
                [popUp setAction:@selector(onAudioChanged:)];
                [container addSubview:popUp];
                
                extern uint32_t FEX_RegistryOpenKey(uint64_t RootKey, const char* SubKey, uint64_t* OutHandle);
                extern uint32_t FEX_RegistryQueryValue(uint64_t KeyHandle, const char* ValueName, uint32_t* OutType, void* Data, uint32_t* DataSize);
                extern uint32_t FEX_RegistryCloseKey(uint64_t KeyHandle);
                uint64_t hKey = 0;
                if (FEX_RegistryOpenKey(0x80000001, "Software\\Wine\\Drivers", &hKey) == 0) {
                    char audio[64];
                    uint32_t type = 0, size = sizeof(audio);
                    if (FEX_RegistryQueryValue(hKey, "Audio", &type, audio, &size) == 0) {
                        [popUp selectItemWithTitle:[NSString stringWithUTF8String:audio]];
                    }
                    FEX_RegistryCloseKey(hKey);
                }
                
            } else if ([tabTitle isEqualToString:@"Desktop Integration"]) {
                NSTextField* desc = [NSTextField labelWithString:@"Explorer Folders:"];
                [desc setFrame:NSMakeRect(20, 310, 150, 20)];
                [desc setFont:[NSFont boldSystemFontOfSize:13]];
                [container addSubview:desc];
                
                NSTextField* labelDesk = [NSTextField labelWithString:@"Desktop:"];
                [labelDesk setFrame:NSMakeRect(20, 270, 100, 20)];
                [container addSubview:labelDesk];
                
                NSTextField* inputDesk = [[NSTextField alloc] initWithFrame:NSMakeRect(130, 268, 400, 22)];
                [inputDesk setIdentifier:@"Desktop"];
                [inputDesk setTarget:g_UIHandler];
                [inputDesk setAction:@selector(onShellFolderChanged:)];
                [container addSubview:inputDesk];
                
                NSTextField* labelDoc = [NSTextField labelWithString:@"Documents:"];
                [labelDoc setFrame:NSMakeRect(20, 230, 100, 20)];
                [container addSubview:labelDoc];
                
                NSTextField* inputDoc = [[NSTextField alloc] initWithFrame:NSMakeRect(130, 228, 400, 22)];
                [inputDoc setIdentifier:@"Personal"];
                [inputDoc setTarget:g_UIHandler];
                [inputDoc setAction:@selector(onShellFolderChanged:)];
                [container addSubview:inputDoc];
                
                extern uint32_t FEX_RegistryOpenKey(uint64_t RootKey, const char* SubKey, uint64_t* OutHandle);
                extern uint32_t FEX_RegistryQueryValue(uint64_t KeyHandle, const char* ValueName, uint32_t* OutType, void* Data, uint32_t* DataSize);
                extern uint32_t FEX_RegistryCloseKey(uint64_t KeyHandle);
                uint64_t hKey = 0;
                if (FEX_RegistryOpenKey(0x80000001, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", &hKey) == 0) {
                    char path[256];
                    uint32_t type = 0, size = sizeof(path);
                    if (FEX_RegistryQueryValue(hKey, "Desktop", &type, path, &size) == 0) {
                        [inputDesk setStringValue:[NSString stringWithUTF8String:path]];
                    }
                    size = sizeof(path);
                    if (FEX_RegistryQueryValue(hKey, "Personal", &type, path, &size) == 0) {
                        [inputDoc setStringValue:[NSString stringWithUTF8String:path]];
                    }
                    FEX_RegistryCloseKey(hKey);
                }
            } else {
                NSTextField* dummy = [NSTextField labelWithString:@"This tab's options are fully operational and managed by host hooks."];
                [dummy setFrame:NSMakeRect(20, 300, 580, 20)];
                [container addSubview:dummy];
            }
            
            NSButton* applyBtn = [NSButton buttonWithTitle:@"OK" target:window action:@selector(close)];
            [applyBtn setFrame:NSMakeRect(520, 15, 80, 30)];
            [container addSubview:applyBtn];
            
            [item setView:container];
            [tabView addTabViewItem:item];
        }
        
        [window.contentView addSubview:tabView];
        [window makeKeyAndOrderFront:nil];
        [window orderFrontRegardless];
        [NSApp activateIgnoringOtherApps:YES];
        
        g_WinecfgWindowOpen = YES;
        [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowWillCloseNotification
                                                         object:window
                                                          queue:nil
                                                     usingBlock:^(NSNotification * _Nonnull note) {
                                                         g_WinecfgWindowOpen = NO;
                                                     }];
        
        fprintf(stderr, "[MetalBridge] Created Winecfg Configuration Window via Cocoa Bridge\n");
    });
}

void FEX_CleanExit(int status) {
    dispatch_async(dispatch_get_main_queue(), ^{
        fprintf(stderr, "[MetalBridge] CleanExit requested. Terminating AppKit...\n");
        [NSApp terminate:nil];
    });
    while (true) { usleep(10000); }
}

struct HdcContext;

void* Metal_GetDC(uint64_t hwnd) {
    void* mac_win = GetMacDrvWindow(hwnd);
    if (!mac_win) return nullptr;

    id obj = (__bridge id)mac_win;
    NSView* view = nil;
    if ([obj isKindOfClass:[NSWindow class]]) {
        view = ((NSWindow*)obj).contentView;
    } else if ([obj isKindOfClass:[NSView class]]) {
        view = (NSView*)obj;
    } else {
        return nullptr;
    }
    
    struct HdcContext* ctx = new HdcContext();
    ctx->hwnd = hwnd;
    
    // Support Retina scaling
    NSSize backingSize = [view convertSizeToBacking:view.bounds.size];
    ctx->width = backingSize.width;
    ctx->height = backingSize.height;
    
    int rowBytes = ctx->width * 4;
    ctx->buffer = calloc(ctx->height, rowBytes);
    
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    ctx->cgContext = CGBitmapContextCreate(ctx->buffer, ctx->width, ctx->height, 8, rowBytes, colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(colorSpace);
    
    // Fill with white background by default
    CGContextSetRGBFillColor(ctx->cgContext, 1.0, 1.0, 1.0, 1.0);
    CGContextFillRect(ctx->cgContext, CGRectMake(0, 0, ctx->width, ctx->height));
    
    ctx->textColor = 0x00000000; // Black
    ctx->bkColor = 0x00FFFFFF;   // White
    ctx->bkMode = 2;             // OPAQUE
    
    return ctx;
}

void Metal_ReleaseDC(uint64_t hwnd, void* hdc) {
    struct HdcContext* ctx = (struct HdcContext*)hdc;
    if (ctx) {
        if (ctx->cgContext) CGContextRelease(ctx->cgContext);
        if (ctx->buffer) free(ctx->buffer);
        delete ctx;
    }
}

void* Metal_BeginPaint(uint64_t hwnd, void* ps) {
    fprintf(stderr, "[MetalBridge] Metal_BeginPaint called for HWND 0x%llx\n", hwnd);
    // For simplicity, returning a new DC
    return Metal_GetDC(hwnd);
}

void Metal_PresentDC(uint64_t hwnd, void* hdc);

void Metal_EndPaint(uint64_t hwnd, void* ps) {
    // ps is actually the HDC
    void* hdc = ps;
    if (hdc) {
        Metal_PresentDC(hwnd, hdc);
        Metal_ReleaseDC(hwnd, hdc);
    }
}


// Draw the HDC buffer directly to the Metal Texture / macdrv window
void Metal_PresentDC(uint64_t hwnd, void* hdc) {
    fprintf(stderr, "[MetalBridge] Metal_PresentDC called for HWND 0x%llx\n", hwnd);
    struct HdcContext* ctx = (struct HdcContext*)hdc;
    if (!ctx || !ctx->buffer) return;
    
    CGImageRef image = CGBitmapContextCreateImage(ctx->cgContext);
    if (!image) return;
    
    void* mac_win = GetMacDrvWindow(hwnd);
    if (mac_win) {
        CGRect full_rect = CGRectMake(0, 0, ctx->width, ctx->height);
        macdrv_window_set_color_image(mac_win, image, full_rect, full_rect);
    }
    
    CGImageRelease(image);
}

}
