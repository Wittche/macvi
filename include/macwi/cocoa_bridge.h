/**
 * @file cocoa_bridge.h
 * @brief C-compatible interface for macOS Cocoa (AppKit) GUI operations.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle representing a native macOS window. */
typedef void* macwi_window_t;

/**
 * Initialize the Cocoa application environment.
 * Must be called from the main thread before any other UI operations.
 */
void macwi_cocoa_init(void);

/**
 * Start the main event loop.
 * This function blocks the main thread permanently (or until quit).
 */
void macwi_cocoa_run_loop(void);

/**
 * Pump events without blocking indefinitely (useful for manual message loops if not using run_loop).
 * Returns true if an event was processed.
 */
bool macwi_cocoa_pump_events(void);

/**
 * Create a new native window.
 * 
 * @param title Window title (UTF-8)
 * @param x X coordinate
 * @param y Y coordinate
 * @param width Width in pixels
 * @param height Height in pixels
 * @return A handle to the created window
 */
macwi_window_t macwi_cocoa_create_window(const char* title, int x, int y, int width, int height);

/**
 * Show or hide a window.
 */
void macwi_cocoa_show_window(macwi_window_t window, bool show);

/**
 * Display a simple message box (alert).
 */
void macwi_cocoa_message_box(const char* title, const char* message);

#ifdef __cplusplus
}
#endif
