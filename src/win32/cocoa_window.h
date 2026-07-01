#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// A simple structure to hold MacWI UI events from Cocoa
typedef enum {
    MACWI_EVENT_NONE = 0,
    MACWI_EVENT_CLOSE = 1,
    MACWI_EVENT_PAINT = 2,
    MACWI_EVENT_KEYDOWN = 3,
    MACWI_EVENT_KEYUP = 4,
    MACWI_EVENT_MOUSEDOWN = 5,
    MACWI_EVENT_MOUSEUP = 6,
    MACWI_EVENT_QUIT = 7
} macwi_event_type_t;

typedef struct {
    macwi_event_type_t type;
    void* window;
    uint32_t key_code;
    int mouse_x;
    int mouse_y;
} macwi_event_t;

// Initialize the Cocoa Application
void macwi_cocoa_init(void);

// Create a Native macOS Window
void* macwi_cocoa_create_window(const char* title, int width, int height);

// Create a Child View
void* macwi_cocoa_create_child_view(void* parent_window, int x, int y, int width, int height);

// Show the Window
void macwi_cocoa_show_window(void* window);

// Process next event. Returns 1 if an event was retrieved, 0 if no events
int macwi_cocoa_poll_event(macwi_event_t* out_event);
int macwi_cocoa_peek_event(macwi_event_t* out_event);

// Draw a filled rectangle in the current context
void macwi_cocoa_fill_rect(void* window, int x, int y, int w, int h, uint32_t argb);

// Draw text in the current context
void macwi_cocoa_draw_text(void* window, int x, int y, const char* text, uint32_t argb, const char* font_name, int font_size);

// Advanced GUI APIs
void macwi_cocoa_get_client_rect(void* window, int* out_w, int* out_h);
void macwi_cocoa_get_window_rect(void* window, int* out_x, int* out_y, int* out_w, int* out_h);
void macwi_cocoa_set_text(void* window, const char* text);
void macwi_cocoa_get_text(void* window, char* out_text, int max_len);
int macwi_cocoa_message_box(void* window, const char* text, const char* caption, uint32_t type);
void macwi_cocoa_end_paint(void);
void macwi_cocoa_post_quit_message(void);

#ifdef __cplusplus
}
#endif
