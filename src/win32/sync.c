#include "macwi/emu.h"
#include "macwi/thunk.h"
#include "macwi/handle.h"
#include "kernel32.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

extern HANDLE_TABLE g_macwi_handle_table;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool manual_reset;
    bool signaled;
} MACWI_EVENT_OBJ;

static void win32_CreateEventA(EMU_CONTEXT* ctx) {
    uint64_t lpEventAttributes, bManualReset, bInitialState, lpName;
    macwi_thunk_read_param_64(ctx, 0, &lpEventAttributes);
    macwi_thunk_read_param_64(ctx, 1, &bManualReset);
    macwi_thunk_read_param_64(ctx, 2, &bInitialState);
    macwi_thunk_read_param_64(ctx, 3, &lpName);

    MACWI_EVENT_OBJ* ev = malloc(sizeof(MACWI_EVENT_OBJ));
    pthread_mutex_init(&ev->mutex, NULL);
    pthread_cond_init(&ev->cond, NULL);
    ev->manual_reset = (bManualReset != 0);
    ev->signaled = (bInitialState != 0);

    HANDLE h = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_EVENT, ev);
    
    printf("[macwi:sync] CreateEventA(manual=%d, init=%d) -> HANDLE 0x%lX\n", (int)ev->manual_reset, (int)ev->signaled, (uint64_t)h);

    macwi_emu_reg_write_64(ctx, 0, (uint64_t)h);
    macwi_thunk_stdcall_return(ctx, 4);
}

static void win32_SetEvent(EMU_CONTEXT* ctx) {
    uint64_t hEvent;
    macwi_thunk_read_param_64(ctx, 0, &hEvent);

    MACWI_EVENT_OBJ* ev = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)hEvent, HANDLE_TYPE_EVENT, (void**)&ev) == MACWI_SUCCESS) {
        pthread_mutex_lock(&ev->mutex);
        ev->signaled = true;
        if (ev->manual_reset) {
            pthread_cond_broadcast(&ev->cond);
        } else {
            pthread_cond_signal(&ev->cond);
        }
        pthread_mutex_unlock(&ev->mutex);
        macwi_emu_reg_write_32(ctx, 0, 1); // TRUE
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0); // FALSE
    }
    
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_ResetEvent(EMU_CONTEXT* ctx) {
    uint64_t hEvent;
    macwi_thunk_read_param_64(ctx, 0, &hEvent);

    MACWI_EVENT_OBJ* ev = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)hEvent, HANDLE_TYPE_EVENT, (void**)&ev) == MACWI_SUCCESS) {
        pthread_mutex_lock(&ev->mutex);
        ev->signaled = false;
        pthread_mutex_unlock(&ev->mutex);
        macwi_emu_reg_write_32(ctx, 0, 1); // TRUE
    } else {
        macwi_emu_reg_write_32(ctx, 0, 0); // FALSE
    }
    
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_WaitForSingleObject(EMU_CONTEXT* ctx) {
    uint64_t hHandle, dwMilliseconds;
    macwi_thunk_read_param_64(ctx, 0, &hHandle);
    macwi_thunk_read_param_64(ctx, 1, &dwMilliseconds);

    // Wait result constants
    #define WAIT_OBJECT_0 0x00000000
    #define WAIT_TIMEOUT  0x00000102
    #define WAIT_FAILED   0xFFFFFFFF

    MACWI_EVENT_OBJ* ev = NULL;
    void* obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)hHandle, HANDLE_TYPE_EVENT, (void**)&ev) == MACWI_SUCCESS) {
        pthread_mutex_lock(&ev->mutex);
        
        int result = WAIT_OBJECT_0;
        
        if (!ev->signaled) {
            if (dwMilliseconds == 0) {
                result = WAIT_TIMEOUT;
            } else if (dwMilliseconds == 0xFFFFFFFF) {
                // INFINITE wait
                while (!ev->signaled) {
                    pthread_cond_wait(&ev->cond, &ev->mutex);
                }
            } else {
                // Timed wait
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                uint64_t msec = dwMilliseconds;
                ts.tv_sec += msec / 1000;
                ts.tv_nsec += (msec % 1000) * 1000000;
                if (ts.tv_nsec >= 1000000000) {
                    ts.tv_sec += 1;
                    ts.tv_nsec -= 1000000000;
                }
                
                int rc = 0;
                while (!ev->signaled && rc == 0) {
                    rc = pthread_cond_timedwait(&ev->cond, &ev->mutex, &ts);
                }
                
                if (rc == ETIMEDOUT && !ev->signaled) {
                    result = WAIT_TIMEOUT;
                }
            }
        }
        
        if (result == WAIT_OBJECT_0 && !ev->manual_reset) {
            // Auto-reset events reset immediately when a waiting thread is released
            ev->signaled = false;
        }
        
        pthread_mutex_unlock(&ev->mutex);
        macwi_emu_reg_write_32(ctx, 0, result);
    } else if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)hHandle, HANDLE_TYPE_MUTEX, &obj) == MACWI_SUCCESS) {
        pthread_mutex_t* m = (pthread_mutex_t*)obj;
        // Simple lock for now (timeout ignored for mutex in this basic impl)
        pthread_mutex_lock(m);
        macwi_emu_reg_write_32(ctx, 0, WAIT_OBJECT_0);
    } else if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)hHandle, HANDLE_TYPE_THREAD, &obj) == MACWI_SUCCESS) {
        pthread_t tid = (pthread_t)(uintptr_t)obj;
        pthread_join(tid, NULL);
        macwi_emu_reg_write_32(ctx, 0, WAIT_OBJECT_0);
    } else {
        // Unknown handle or type
        printf("[macwi:sync] WaitForSingleObject(0x%lX) FAILED (unknown handle)\n", (uint64_t)hHandle);
        macwi_emu_reg_write_32(ctx, 0, WAIT_FAILED);
    }

    macwi_thunk_stdcall_return(ctx, 2);
}

void macwi_sync_register_apis(void) {
    macwi_thunk_register_api("kernel32.dll", "CreateEventA", win32_CreateEventA, 4);
    macwi_thunk_register_api("kernel32.dll", "SetEvent", win32_SetEvent, 1);
    macwi_thunk_register_api("kernel32.dll", "ResetEvent", win32_ResetEvent, 1);
    macwi_thunk_register_api("kernel32.dll", "WaitForSingleObject", win32_WaitForSingleObject, 2);
}
