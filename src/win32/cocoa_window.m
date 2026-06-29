#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#include "cocoa_window.h"

#import <pthread.h>

// A queue to store translated events to feed into C GetMessage
static NSMutableArray* g_eventQueue = nil;
static pthread_mutex_t g_event_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_paint_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_paint_cond = PTHREAD_COND_INITIALIZER;
static CGContextRef g_current_cg_context = NULL;
static bool g_in_draw_rect = false;

@interface MacWIWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation MacWIWindowDelegate

- (BOOL)windowShouldClose:(NSWindow *)sender {
    pthread_mutex_lock(&g_event_mutex);
    if (!g_eventQueue) {
        pthread_mutex_unlock(&g_event_mutex);
        return YES;
    }
    
    // Push close event
    macwi_event_t event;
    event.type = MACWI_EVENT_CLOSE;
    event.window = (void*)sender;
    event.key_code = 0;
    event.mouse_x = 0;
    event.mouse_y = 0;
    
    NSValue* eventVal = [NSValue valueWithBytes:&event objCType:@encode(macwi_event_t)];
    [g_eventQueue addObject:eventVal];
    pthread_mutex_unlock(&g_event_mutex);
    
    return NO; // We don't close immediately, let Win32 DestroyWindow do it
}

@end

@interface MacWIView : NSView
@end

@implementation MacWIView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint loc = [self convertPoint:[event locationInWindow] fromView:nil];
    macwi_event_t out_evt;
    out_evt.type = MACWI_EVENT_MOUSEDOWN;
    out_evt.window = (void*)[self window];
    out_evt.key_code = 0;
    out_evt.mouse_x = (int)loc.x;
    out_evt.mouse_y = (int)([self bounds].size.height - loc.y); // Convert Cocoa Y to Windows Y (top-down)

    pthread_mutex_lock(&g_event_mutex);
    if (g_eventQueue) {
        NSValue* val = [NSValue valueWithBytes:&out_evt objCType:@encode(macwi_event_t)];
        [g_eventQueue addObject:val];
    }
    pthread_mutex_unlock(&g_event_mutex);
}

- (void)mouseUp:(NSEvent *)event {
    NSPoint loc = [self convertPoint:[event locationInWindow] fromView:nil];
    macwi_event_t out_evt;
    out_evt.type = MACWI_EVENT_MOUSEUP;
    out_evt.window = (void*)[self window];
    out_evt.key_code = 0;
    out_evt.mouse_x = (int)loc.x;
    out_evt.mouse_y = (int)([self bounds].size.height - loc.y);

    pthread_mutex_lock(&g_event_mutex);
    if (g_eventQueue) {
        NSValue* val = [NSValue valueWithBytes:&out_evt objCType:@encode(macwi_event_t)];
        [g_eventQueue addObject:val];
    }
    pthread_mutex_unlock(&g_event_mutex);
}

- (void)keyDown:(NSEvent *)event {
    macwi_event_t out_evt;
    out_evt.type = MACWI_EVENT_KEYDOWN;
    out_evt.window = (void*)[self window];
    out_evt.key_code = [event keyCode];
    out_evt.mouse_x = 0;
    out_evt.mouse_y = 0;

    pthread_mutex_lock(&g_event_mutex);
    if (g_eventQueue) {
        NSValue* val = [NSValue valueWithBytes:&out_evt objCType:@encode(macwi_event_t)];
        [g_eventQueue addObject:val];
    }
    pthread_mutex_unlock(&g_event_mutex);
}

- (void)keyUp:(NSEvent *)event {
    macwi_event_t out_evt;
    out_evt.type = MACWI_EVENT_KEYUP;
    out_evt.window = (void*)[self window];
    out_evt.key_code = [event keyCode];
    out_evt.mouse_x = 0;
    out_evt.mouse_y = 0;

    pthread_mutex_lock(&g_event_mutex);
    if (g_eventQueue) {
        NSValue* val = [NSValue valueWithBytes:&out_evt objCType:@encode(macwi_event_t)];
        [g_eventQueue addObject:val];
    }
    pthread_mutex_unlock(&g_event_mutex);
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    
    g_current_cg_context = [[NSGraphicsContext currentContext] CGContext];
    g_in_draw_rect = true;
    
    pthread_mutex_lock(&g_event_mutex);
    if (!g_eventQueue) {
        pthread_mutex_unlock(&g_event_mutex);
        g_current_cg_context = NULL;
        g_in_draw_rect = false;
        return;
    }
    
    // Push paint event
    macwi_event_t event;
    event.type = MACWI_EVENT_PAINT;
    event.window = (void*)[self window];
    event.key_code = 0;
    event.mouse_x = 0;
    event.mouse_y = 0;

    NSValue* eventVal = [NSValue valueWithBytes:&event objCType:@encode(macwi_event_t)];
    [g_eventQueue addObject:eventVal];
    pthread_mutex_unlock(&g_event_mutex);
    
    // Wait until FEXCore thread finishes painting (via EndPaint)
    pthread_mutex_lock(&g_paint_mutex);
    pthread_cond_wait(&g_paint_cond, &g_paint_mutex);
    pthread_mutex_unlock(&g_paint_mutex);
    
    g_current_cg_context = NULL;
    g_in_draw_rect = false;
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
    dispatch_async(dispatch_get_main_queue(), ^{
        NSWindow* window = (__bridge NSWindow*)window_ptr;
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    });
}

int macwi_cocoa_poll_event(macwi_event_t* out_event) {
    int result = 0;
    
    // 1. Check custom queue without blocking main queue
    pthread_mutex_lock(&g_event_mutex);
    if (g_eventQueue && [g_eventQueue count] > 0) {
        NSValue* val = [g_eventQueue firstObject];
        [val getValue:out_event];
        [g_eventQueue removeObjectAtIndex:0];
        result = 1;
    }
    pthread_mutex_unlock(&g_event_mutex);
    
    if (result) return 1;
    
    // Cocoa events are already processed by [NSApp run] on the main thread,
    // which then pushes them to g_eventQueue via our delegate methods.
    // There is no need to manually poll Cocoa events here, and doing so via
    // dispatch_sync causes deadlocks with drawRect:.
    
    // Check custom queue again
    pthread_mutex_lock(&g_event_mutex);
    if (g_eventQueue && [g_eventQueue count] > 0) {
        NSValue* val = [g_eventQueue firstObject];
        [val getValue:out_event];
        [g_eventQueue removeObjectAtIndex:0];
        result = 1;
    }
    pthread_mutex_unlock(&g_event_mutex);
    
    return result;
}

void macwi_cocoa_end_paint(void) {
    pthread_mutex_lock(&g_paint_mutex);
    pthread_cond_signal(&g_paint_cond);
    pthread_mutex_unlock(&g_paint_mutex);
}

void macwi_cocoa_fill_rect(void* window_ptr, int x, int y, int w, int h, uint32_t argb) {
    void (^drawBlock)(void) = ^{
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
        if (g_current_cg_context) {
            NSGraphicsContext* ctx = [NSGraphicsContext graphicsContextWithCGContext:g_current_cg_context flipped:NO];
            [NSGraphicsContext setCurrentContext:ctx];
        }
        
        [color setFill];
        NSRectFill(rect);
        
        [NSGraphicsContext restoreGraphicsState];
    };
    
    if (g_current_cg_context) {
        drawBlock();
    } else {
        dispatch_sync(dispatch_get_main_queue(), drawBlock);
    }
}

void macwi_cocoa_draw_text(void* window_ptr, int x, int y, const char* text, uint32_t argb) {
    void (^drawBlock)(void) = ^{
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
        
        [NSGraphicsContext saveGraphicsState];
        if (g_current_cg_context) {
            NSGraphicsContext* ctx = [NSGraphicsContext graphicsContextWithCGContext:g_current_cg_context flipped:NO];
            [NSGraphicsContext setCurrentContext:ctx];
        }
        
        [str drawAtPoint:point withAttributes:attrs];
        
        [NSGraphicsContext restoreGraphicsState];
    };
    
    if (g_current_cg_context) {
        drawBlock();
    } else {
        dispatch_sync(dispatch_get_main_queue(), drawBlock);
    }
}

void macwi_cocoa_get_client_rect(void* window_ptr, int* out_w, int* out_h) {
    if (!window_ptr) return;
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSWindow* window = (__bridge NSWindow*)window_ptr;
        NSRect rect = [[window contentView] bounds];
        if (out_w) *out_w = (int)rect.size.width;
        if (out_h) *out_h = (int)rect.size.height;
    });
}

void macwi_cocoa_get_window_rect(void* window_ptr, int* out_x, int* out_y, int* out_w, int* out_h) {
    if (!window_ptr) return;
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSWindow* window = (__bridge NSWindow*)window_ptr;
        NSRect rect = [window frame];
        if (out_x) *out_x = (int)rect.origin.x;
        // Mac coordinates are bottom-left, Windows are top-left, approximate for now
        if (out_y) *out_y = (int)([[NSScreen mainScreen] frame].size.height - rect.origin.y - rect.size.height);
        if (out_w) *out_w = (int)rect.size.width;
        if (out_h) *out_h = (int)rect.size.height;
    });
}

void macwi_cocoa_set_text(void* window_ptr, const char* text) {
    if (!window_ptr || !text) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        NSWindow* window = (__bridge NSWindow*)window_ptr;
        [window setTitle:[NSString stringWithUTF8String:text]];
    });
}

void macwi_cocoa_get_text(void* window_ptr, char* out_text, int max_len) {
    if (!window_ptr || !out_text || max_len <= 0) return;
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSWindow* window = (__bridge NSWindow*)window_ptr;
        NSString* title = [window title];
        strncpy(out_text, [title UTF8String], max_len - 1);
        out_text[max_len - 1] = '\0';
    });
}

int macwi_cocoa_message_box(void* window_ptr, const char* text, const char* caption, uint32_t type) {
    __block NSModalResponse response = 0;
    
    // Cannot block FEXCore directly, but we can wait since we're in a host syscall
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:caption ? caption : "Message"]];
        [alert setInformativeText:[NSString stringWithUTF8String:text ? text : ""]];
        
        // Simple type handling: MB_OK (0), MB_OKCANCEL (1), MB_YESNO (4)
        if ((type & 0xF) == 1) { // MB_OKCANCEL
            [alert addButtonWithTitle:@"OK"];
            [alert addButtonWithTitle:@"Cancel"];
        } else if ((type & 0xF) == 4) { // MB_YESNO
            [alert addButtonWithTitle:@"Yes"];
            [alert addButtonWithTitle:@"No"];
        } else { // Default to MB_OK
            [alert addButtonWithTitle:@"OK"];
        }
        
        if (window_ptr) {
            NSWindow* window = (__bridge NSWindow*)window_ptr;
            [alert beginSheetModalForWindow:window completionHandler:^(NSModalResponse res) {
                // Not ideal since it's async, we actually need blocking here for Win32 semantics
            }];
        } else {
            response = [alert runModal];
        }
    });
    
    // For blocking behavior with window, runModal is needed
    if (window_ptr) {
        dispatch_sync(dispatch_get_main_queue(), ^{
             NSAlert* alert = [[NSAlert alloc] init];
             [alert setMessageText:[NSString stringWithUTF8String:caption ? caption : "Message"]];
             [alert setInformativeText:[NSString stringWithUTF8String:text ? text : ""]];
             
             if ((type & 0xF) == 1) { // MB_OKCANCEL
                 [alert addButtonWithTitle:@"OK"];
                 [alert addButtonWithTitle:@"Cancel"];
             } else if ((type & 0xF) == 4) { // MB_YESNO
                 [alert addButtonWithTitle:@"Yes"];
                 [alert addButtonWithTitle:@"No"];
             } else { // Default to MB_OK
                 [alert addButtonWithTitle:@"OK"];
             }
             
             response = [alert runModal];
        });
    }
    
    // Map NSAlertFirstButtonReturn etc to IDOK (1), IDCANCEL (2), IDYES (6), IDNO (7)
    if ((type & 0xF) == 1) {
        return (response == NSAlertFirstButtonReturn) ? 1 : 2;
    } else if ((type & 0xF) == 4) {
        return (response == NSAlertFirstButtonReturn) ? 6 : 7;
    }
    return 1; // IDOK
}
