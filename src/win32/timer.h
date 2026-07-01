#pragma once

#include "macwi/types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t id;          // Timer ID (nIDEvent or generated if hWnd is NULL)
    uint64_t hwnd;        // HWND associated with this timer
    uint64_t elapse;      // Interval in milliseconds
    uint64_t timer_func;  // Callback address in guest
    uint64_t last_tick;   // Last triggered time in nanoseconds
    int active;
} MACWI_TIMER;

#define MAX_TIMERS 256

void macwi_timer_init(void);
uint64_t macwi_timer_set(uint64_t hwnd, uint64_t idEvent, uint64_t uElapse, uint64_t lpTimerFunc);
int macwi_timer_kill(uint64_t hwnd, uint64_t idEvent);
int macwi_timer_check(uint64_t* out_hwnd, uint64_t* out_idEvent, uint64_t* out_timerFunc);
uint64_t macwi_timer_get_sys_time(void);

#ifdef __cplusplus
}
#endif
