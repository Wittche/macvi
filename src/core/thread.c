/**
 * @file thread.c
 * @brief Win32 Thread and TEB management using pthreads.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/thread.h"
#include "macwi/emu.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static uint32_t g_next_thread_id = 1000;

macwi_status_t macwi_thread_init_subsystem(void) {
    return MACWI_SUCCESS;
}

// POSIX thread entry point
static void* host_thread_routine(void* arg) {
    MacWIThread* th = (MacWIThread*)arg;
    
    // In a full implementation, we would instantiate a new EMU_CONTEXT here,
    // map the shared guest memory, setup the TEB in the %fs register,
    // push lpParameter onto the stack, and call uc_emu_start(th->entry_point).
    
    fprintf(stderr, "[macwi_thread] Thread %u started (entry=0x%X, param=0x%X)\n", 
            th->thread_id, th->entry_point, th->lpParameter);
            
    // Simulate some work
    usleep(100000); 

    th->is_running = 0;
    th->exit_code = 0;
    
    fprintf(stderr, "[macwi_thread] Thread %u exited.\n", th->thread_id);
    return NULL;
}

macwi_status_t macwi_thread_create(EMU_CONTEXT* parent_ctx, uint32_t entry_point, uint32_t lpParameter, MacWIThread** out_thread) {
    MacWIThread* th = (MacWIThread*)calloc(1, sizeof(MacWIThread));
    if (!th) return MACWI_ERROR_MEMORY;
    
    th->thread_id = __atomic_fetch_add(&g_next_thread_id, 1, __ATOMIC_SEQ_CST);
    th->process_id = 100; // Hardcoded main PID
    th->entry_point = entry_point;
    th->lpParameter = lpParameter;
    th->is_running = 1;
    
    // Create the host pthread
    if (pthread_create(&th->host_thread, NULL, host_thread_routine, th) != 0) {
        free(th);
        return MACWI_ERROR_IO;
    }
    
    *out_thread = th;
    return MACWI_SUCCESS;
}
