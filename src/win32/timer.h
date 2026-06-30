#pragma once

#include "macwi/types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t id;          // Timer ID (nIDEvent or generated if hWnd is NULL)
    uint32_t hwnd;        // HWND associated with this timer
    uint32_t elapse;      // Interval in milliseconds
    uint32_t timer_func;  // Callback address in guest
    uint64_t last_tick;   // Last triggered time in nanoseconds
    int active;
} MACWI_TIMER;

#define MAX_TIMERS 256

void macwi_timer_init(void);
uint32_t macwi_timer_set(uint32_t hwnd, uint32_t idEvent, uint32_t uElapse, uint32_t lpTimerFunc);
int macwi_timer_kill(uint32_t hwnd, uint32_t idEvent);
int macwi_timer_check(uint32_t* out_hwnd, uint32_t* out_idEvent, uint32_t* out_timerFunc);
uint32_t macwi_timer_get_sys_time(void);

#ifdef __cplusplus
}
#endif
