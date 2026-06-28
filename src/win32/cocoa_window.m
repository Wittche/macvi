#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#include "cocoa_window.h"

// A queue to store translated events to feed into C GetMessage
static NSMutableArray* g_eventQueue = nil;

@interface MacWIWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation MacWIWindowDelegate

- (BOOL)windowShouldClose:(NSWindow *)sender {
    if (!g_eventQueue) return YES;
    
    // Push close event
    macwi_event_t event;
    event.type = MACWI_EVENT_CLOSE;
    event.window = (void*)sender;
    event.key_code = 0;
    event.mouse_x = 0;
    event.mouse_y = 0;
    
    NSValue* eventVal = [NSValue valueWithBytes:&event objCType:@encode(macwi_event_t)];
    [g_eventQueue addObject:eventVal];
    return NO; // We don't close immediately, let Win32 DestroyWindow do it
}

@end

@interface MacWIView : NSView
@end

@implementation MacWIView

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    
    if (!g_eventQueue) return;
    
    // Push paint event
    macwi_event_t event;
    event.type = MACWI_EVENT_PAINT;
    event.window = (void*)[self window];
    event.key_code = 0;
    event.mouse_x = 0;
    event.mouse_y = 0;

    NSValue* eventVal = [NSValue valueWithBytes:&event objCType:@encode(macwi_event_t)];
    [g_eventQueue addObject:eventVal];
}

@end

void macwi_cocoa_init(void) {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    g_eventQueue = [[NSMutableArray alloc] init];
}

void macwi_cocoa_run_loop(void) {
    [NSApp run];
}

void* macwi_cocoa_create_window(const char* title, int width, int height) {
    __block NSWindow* window = nil;
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSRect frame = NSMakeRect(0, 0, width, height);
        NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
        
        window = [[NSWindow alloc] initWithContentRect:frame
                                                     styleMask:style
                                                       backing:NSBackingStoreBuffered
                                                         defer:NO];
        
        [window setTitle:[NSString stringWithUTF8String:title]];
        [window center];
        
        MacWIWindowDelegate* delegate = [[MacWIWindowDelegate alloc] init];
        [window setDelegate:delegate];
        
        objc_setAssociatedObject(window, "MacWIDelegate", delegate, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        
        MacWIView* view = [[MacWIView alloc] initWithFrame:frame];
        [window setContentView:view];
    });
    return (void*)window;
}

void macwi_cocoa_show_window(void* window_ptr) {
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSWindow* window = (__bridge NSWindow*)window_ptr;
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    });
}

int macwi_cocoa_poll_event(macwi_event_t* out_event) {
    __block int result = 0;
    __block macwi_event_t ev;
    
    dispatch_sync(dispatch_get_main_queue(), ^{
        if (!g_eventQueue) return;
        
        // 1. First, drain our custom queue
        if ([g_eventQueue count] > 0) {
            NSValue* val = [g_eventQueue firstObject];
            [val getValue:&ev];
            [g_eventQueue removeObjectAtIndex:0];
            result = 1;
            return;
        }
        
        // 2. Poll Cocoa events (non-blocking)
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        
        if (event) {
            [NSApp sendEvent:event];
        }
        
        // Check custom queue again
        if ([g_eventQueue count] > 0) {
            NSValue* val = [g_eventQueue firstObject];
            [val getValue:&ev];
            [g_eventQueue removeObjectAtIndex:0];
            result = 1;
        }
    });
    
    if (result) {
        *out_event = ev;
    }
    return result;
}

void macwi_cocoa_fill_rect(void* window_ptr, int x, int y, int w, int h, uint32_t argb) {
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSWindow* window = (__bridge NSWindow*)window_ptr;
        NSView* view = [window contentView];
        
        NSRect viewRect = [view bounds];
        NSRect rect = NSMakeRect(x, viewRect.size.height - y - h, w, h);
        
        CGFloat r = ((argb >> 16) & 0xFF) / 255.0;
        CGFloat g = ((argb >> 8) & 0xFF) / 255.0;
        CGFloat b = (argb & 0xFF) / 255.0;
        CGFloat a = ((argb >> 24) & 0xFF) / 255.0;
        if (a == 0.0) a = 1.0;
        
        NSColor* color = [NSColor colorWithDeviceRed:r green:g blue:b alpha:a];
        
        [NSGraphicsContext saveGraphicsState];
        [color setFill];
        NSRectFill(rect);
        [NSGraphicsContext restoreGraphicsState];
    });
}

void macwi_cocoa_draw_text(void* window_ptr, int x, int y, const char* text, uint32_t argb) {
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSWindow* window = (__bridge NSWindow*)window_ptr;
        NSView* view = [window contentView];
        
        NSString* str = [NSString stringWithUTF8String:text];
        
        CGFloat r = ((argb >> 16) & 0xFF) / 255.0;
        CGFloat g = ((argb >> 8) & 0xFF) / 255.0;
        CGFloat b = (argb & 0xFF) / 255.0;
        CGFloat a = ((argb >> 24) & 0xFF) / 255.0;
        if (a == 0.0) a = 1.0;
        NSColor* color = [NSColor colorWithDeviceRed:r green:g blue:b alpha:a];
        
        NSDictionary* attrs = @{
            NSFontAttributeName: [NSFont systemFontOfSize:14.0],
            NSForegroundColorAttributeName: color
        };
        
        NSRect viewRect = [view bounds];
        NSSize size = [str sizeWithAttributes:attrs];
        NSPoint point = NSMakePoint(x, viewRect.size.height - y - size.height);
        
        [str drawAtPoint:point withAttributes:attrs];
    });
}
