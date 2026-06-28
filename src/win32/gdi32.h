#pragma once

#include "macwi/emu.h"

#ifdef __cplusplus
extern "C" {
#endif

// Register GDI32 APIs
void macwi_gdi32_register_apis(void);

// GDI constants
#define WHITE_BRUSH         0
#define LTGRAY_BRUSH        1
#define GRAY_BRUSH          2
#define DKGRAY_BRUSH        3
#define BLACK_BRUSH         4
#define NULL_BRUSH          5

typedef enum {
    GDI_OBJ_BRUSH = 1,
    GDI_OBJ_FONT  = 2,
    GDI_OBJ_PEN   = 3
} MACWI_GDI_OBJ_TYPE;

typedef struct {
    MACWI_GDI_OBJ_TYPE type;
    uint32_t argb;
} MACWI_GDI_OBJ;

typedef struct {
    void* cocoa_window;
    HANDLE current_brush;
    HANDLE current_pen;
    HANDLE current_font;
    uint32_t text_color;
    uint32_t bk_color;
} MACWI_HDC_OBJ;

typedef uint32_t HDC32;
typedef uint32_t HBRUSH32;

typedef struct {
    HDC32 hdc;
    int32_t fErase;
    int32_t rcPaint_left;
    int32_t rcPaint_top;
    int32_t rcPaint_right;
    int32_t rcPaint_bottom;
    int32_t fRestore;
    int32_t fIncUpdate;
    uint8_t rgbReserved[32];
} PAINTSTRUCT_32;

typedef struct {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} RECT_32;

#ifdef __cplusplus
}
#endif
