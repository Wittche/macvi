#import <Cocoa/Cocoa.h>
#include <stdio.h>

void macwi_cocoa_show_window_logged(void* window_ptr) {
    printf("[macwi:cocoa] macwi_cocoa_show_window called with window_ptr=%p\n", window_ptr);
    dispatch_async(dispatch_get_main_queue(), ^{
        NSView* view = (__bridge NSView*)window_ptr;
        NSWindow* window = [view window];
        printf("[macwi:cocoa] window from view=%p is %p. Is view an NSView? %s\n", view, window, [view isKindOfClass:[NSView class]] ? "YES" : "NO");
        if (window) {
            printf("[macwi:cocoa] Window frame: %f, %f, %f, %f\n", window.frame.origin.x, window.frame.origin.y, window.frame.size.width, window.frame.size.height);
            [window makeKeyAndOrderFront:nil];
            [NSApp activateIgnoringOtherApps:YES];
            printf("[macwi:cocoa] Window ordered front and app activated.\n");
        } else {
            printf("[macwi:cocoa] ERROR: Window is nil! Cannot show window.\n");
        }
        fflush(stdout);
    });
}
