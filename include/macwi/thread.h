/**
 * @file thread.h
 * @brief Thread and TEB (Thread Environment Block) structures.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "macwi/types.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration of emulator context
typedef struct EMU_CONTEXT EMU_CONTEXT;

// Simplified Thread Environment Block (TEB)
typedef struct _TEB_32 {
    uint32_t ExceptionList;    // 0x00
    uint32_t StackBase;        // 0x04
    uint32_t StackLimit;       // 0x08
    uint32_t SubSystemTib;     // 0x0C
    uint32_t FiberData;        // 0x10
    uint32_t ArbitraryUser;    // 0x14
    uint32_t TebAddress;       // 0x18 - Self pointer (Important for FS:0x18)
    uint32_t EnvironmentPointer; // 0x1C
    uint32_t ClientIdProcess;  // 0x20
    uint32_t ClientIdThread;   // 0x24
    uint32_t RpcHandle;        // 0x28
    uint32_t TlsSlots[64];     // 0x2C
    uint32_t LastErrorValue;   // 0x34 (approx)
} TEB_32;

typedef struct MacWIThread {
    pthread_t host_thread;
    uint32_t thread_id;
    uint32_t process_id;
    
    // Each thread needs its own CPU registers
    EMU_CONTEXT* ctx;
    
    // Stack boundaries
    uint32_t stack_base;
    uint32_t stack_size;
    
    // TEB mapped address in guest memory
    uint32_t teb_address;
    
    // Thread state
    int is_running;
    uint32_t exit_code;
    
    // Entry point provided by CreateThread
    uint32_t entry_point;
    uint32_t lpParameter;
} MacWIThread;

macwi_status_t macwi_thread_init_subsystem(void);
macwi_status_t macwi_thread_create(EMU_CONTEXT* parent_ctx, uint32_t entry_point, uint32_t lpParameter, MacWIThread** out_thread);

#ifdef __cplusplus
}
#endif
