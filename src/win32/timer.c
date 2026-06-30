#include "timer.h"
#include <pthread.h>
#include <time.h>
#include <string.h>

static MACWI_TIMER g_timers[MAX_TIMERS];
static pthread_mutex_t g_timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_next_timer_id = 0x1000;

static uint64_t get_current_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void macwi_timer_init(void) {
    memset(g_timers, 0, sizeof(g_timers));
}

uint32_t macwi_timer_set(uint32_t hwnd, uint32_t idEvent, uint32_t uElapse, uint32_t lpTimerFunc) {
    pthread_mutex_lock(&g_timer_mutex);
    
    // Find existing timer if hwnd and idEvent match
    if (hwnd != 0) {
        for (int i = 0; i < MAX_TIMERS; i++) {
            if (g_timers[i].active && g_timers[i].hwnd == hwnd && g_timers[i].id == idEvent) {
                g_timers[i].elapse = uElapse;
                g_timers[i].timer_func = lpTimerFunc;
                g_timers[i].last_tick = get_current_time_ns();
                pthread_mutex_unlock(&g_timer_mutex);
                return idEvent;
            }
        }
    }
    
    // Allocate new timer
    uint32_t assigned_id = idEvent;
    if (hwnd == 0) {
        assigned_id = g_next_timer_id++;
    }
    
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!g_timers[i].active) {
            g_timers[i].active = 1;
            g_timers[i].hwnd = hwnd;
            g_timers[i].id = assigned_id;
            g_timers[i].elapse = uElapse;
            g_timers[i].timer_func = lpTimerFunc;
            g_timers[i].last_tick = get_current_time_ns();
            pthread_mutex_unlock(&g_timer_mutex);
            return assigned_id;
        }
    }
    
    pthread_mutex_unlock(&g_timer_mutex);
    return 0; // Failure
}

int macwi_timer_kill(uint32_t hwnd, uint32_t idEvent) {
    pthread_mutex_lock(&g_timer_mutex);
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (g_timers[i].active && g_timers[i].hwnd == hwnd && g_timers[i].id == idEvent) {
            g_timers[i].active = 0;
            pthread_mutex_unlock(&g_timer_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_timer_mutex);
    return 0; // Not found
}

int macwi_timer_check(uint32_t* out_hwnd, uint32_t* out_idEvent, uint32_t* out_timerFunc) {
    uint64_t now = get_current_time_ns();
    
    pthread_mutex_lock(&g_timer_mutex);
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (g_timers[i].active) {
            uint64_t elapsed_ms = (now - g_timers[i].last_tick) / 1000000ULL;
            if (elapsed_ms >= g_timers[i].elapse) {
                // Timer expired
                g_timers[i].last_tick = now; // reset tick
                
                if (out_hwnd) *out_hwnd = g_timers[i].hwnd;
                if (out_idEvent) *out_idEvent = g_timers[i].id;
                if (out_timerFunc) *out_timerFunc = g_timers[i].timer_func;
                
                pthread_mutex_unlock(&g_timer_mutex);
                return 1;
            }
        }
    }
    pthread_mutex_unlock(&g_timer_mutex);
    return 0;
}

uint32_t macwi_timer_get_sys_time(void) {
    return (uint32_t)(get_current_time_ns() / 1000000ULL);
}
