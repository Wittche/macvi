#pragma once

#include "macwi/emu.h"

#ifdef __cplusplus
extern "C" {
#endif

// Register GDI32 APIs
void macwi_gdi32_register_apis(void);

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
