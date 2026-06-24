/**
 * @file cocoa_bridge.m
 * @brief Cocoa (AppKit) implementation of the macOS GUI bridge.
 *
 * SPDX-License-Identifier: MIT
 */

#import <Cocoa/Cocoa.h>
#include "macwi/cocoa_bridge.h"

// Define a custom application delegate to handle app lifecycle
@interface MacWIAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation MacWIAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Set up default menu bar
    NSMenu *menubar = [[NSMenu alloc] init];
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    [menubar addItem:appMenuItem];
    [NSApp setMainMenu:menubar];

    NSMenu *appMenu = [[NSMenu alloc] init];
    NSString *appName = [[NSProcessInfo processInfo] processName];
    NSString *quitTitle = [NSString stringWithFormat:@"Quit %@", appName];
    NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];
    
    // Ensure the app comes to the front
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return NO; // Let the emulated app decide when to quit
}
@end

// ----------------------------------------------------------------------------
// C API Implementation
// ----------------------------------------------------------------------------

void macwi_cocoa_init(void) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        
        MacWIAppDelegate *delegate = [[MacWIAppDelegate alloc] init];
        [NSApp setDelegate:delegate];
    }
}

void macwi_cocoa_run_loop(void) {
    @autoreleasepool {
        [NSApp run];
    }
}

bool macwi_cocoa_pump_events(void) {
    @autoreleasepool {
        NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event) {
            [NSApp sendEvent:event];
            return true;
        }
        return false;
    }
}

macwi_window_t macwi_cocoa_create_window(const char* title, int x, int y, int width, int height) {
    __block NSWindow* window = nil;
    
    // UI operations should happen on the main thread
    dispatch_sync(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            NSRect frame = NSMakeRect(x, y, width, height);
            
            // Typical Win32 WS_OVERLAPPEDWINDOW maps to these style masks
            NSWindowStyleMask style = NSWindowStyleMaskTitled | 
                                      NSWindowStyleMaskClosable | 
                                      NSWindowStyleMaskMiniaturizable | 
                                      NSWindowStyleMaskResizable;
            
            window = [[NSWindow alloc] initWithContentRect:frame
                                                 styleMask:style
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
            
            NSString* nsTitle = [NSString stringWithUTF8String:title ? title : "MacWI Window"];
            [window setTitle:nsTitle];
            [window center];
        }
    });
    
    return (__bridge_retained void*)window;
}

void macwi_cocoa_show_window(macwi_window_t window_ptr, bool show) {
    if (!window_ptr) return;
    
    // Transfer back to ARC temporarily
    NSWindow* window = (__bridge NSWindow*)window_ptr;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            if (show) {
                [window makeKeyAndOrderFront:nil];
            } else {
                [window orderOut:nil];
            }
        }
    });
}

void macwi_cocoa_message_box(const char* title, const char* message) {
    NSString* nsTitle = [NSString stringWithUTF8String:title ? title : "Message"];
    NSString* nsMessage = [NSString stringWithUTF8String:message ? message : ""];
    
    dispatch_sync(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            NSAlert *alert = [[NSAlert alloc] init];
            [alert setMessageText:nsTitle];
            [alert setInformativeText:nsMessage];
            [alert addButtonWithTitle:@"OK"];
            [alert setAlertStyle:NSAlertStyleInformational];
            [alert runModal];
        }
    });
}
