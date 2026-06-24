/**
 * @file kernel32.c
 * @brief Win32 kernel32.dll stub implementations using the EMU_CONTEXT dispatcher.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/types.h"
#include "macwi/emu.h"
#include "macwi/thunk.h"
#include "macwi/vfs.h"
#include "macwi/handle.h"
#include "macwi/thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#define STUB_LOG(fmt, ...) fprintf(stderr, "[macwi:kernel32] " fmt "\n", ##__VA_ARGS__)

/* ============================================================================
 * Thread-local last-error code
 * ============================================================================ */
static _Thread_local uint32_t tls_last_error = 0;

static void set_last_error(uint32_t err) { tls_last_error = err; }

static void win32_GetLastError(EMU_CONTEXT* ctx) {
    macwi_emu_reg_write(ctx, 0 /* EAX */, tls_last_error);
}

static void win32_SetLastError(EMU_CONTEXT* ctx) {
    uint32_t err;
    macwi_thunk_read_param_32(ctx, 0, &err);
    set_last_error(err);
    macwi_emu_reg_write(ctx, 0, 0);
}

/* ============================================================================
 * Debugging and timing
 * ============================================================================ */

static void win32_GetTickCount(EMU_CONTEXT* ctx) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t ticks = (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    macwi_emu_reg_write(ctx, 0, ticks);
}

static void win32_Sleep(EMU_CONTEXT* ctx) {
    uint32_t ms;
    macwi_thunk_read_param_32(ctx, 0, &ms);
    STUB_LOG("Sleep(%u ms)", ms);
    usleep(ms * 1000);
    macwi_emu_reg_write(ctx, 0, 0);
}

static void win32_OutputDebugStringA(EMU_CONTEXT* ctx) {
    uint32_t lpOutputString;
    macwi_thunk_read_param_32(ctx, 0, &lpOutputString);
    char buf[1024];
    macwi_thunk_read_guest_string(ctx, lpOutputString, buf, sizeof(buf));
    STUB_LOG("OutputDebugStringA: %s", buf);
    macwi_emu_reg_write(ctx, 0, 0);
}

static void win32_lstrlenA(EMU_CONTEXT* ctx) {
    uint32_t lpString;
    macwi_thunk_read_param_32(ctx, 0, &lpString);
    if (!lpString) {
        macwi_emu_reg_write(ctx, 0, 0);
        return;
    }
    // We need to find the length of the string in guest memory.
    // For now, read it into a host buffer up to 4096 chars and use strlen.
    char buf[4096];
    macwi_thunk_read_guest_string(ctx, lpString, buf, sizeof(buf));
    macwi_emu_reg_write(ctx, 0, strlen(buf));
}

/* ============================================================================
 * File I/O
 * ============================================================================ */

static void win32_CreateFileA(EMU_CONTEXT* ctx) {
    uint32_t lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes;
    uint32_t dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile;
    
    macwi_thunk_read_param_32(ctx, 0, &lpFileName);
    macwi_thunk_read_param_32(ctx, 1, &dwDesiredAccess);
    macwi_thunk_read_param_32(ctx, 2, &dwShareMode);
    macwi_thunk_read_param_32(ctx, 3, &lpSecurityAttributes);
    macwi_thunk_read_param_32(ctx, 4, &dwCreationDisposition);
    macwi_thunk_read_param_32(ctx, 5, &dwFlagsAndAttributes);
    macwi_thunk_read_param_32(ctx, 6, &hTemplateFile);

    char filename[256];
    macwi_thunk_read_guest_string(ctx, lpFileName, filename, sizeof(filename));
    
    char unix_path[MACWI_MAX_PATH];
    if (macwi_vfs_dos_to_unix(filename, unix_path) != MACWI_SUCCESS) {
        set_last_error(3); // ERROR_PATH_NOT_FOUND
        macwi_emu_reg_write(ctx, 0, 0xFFFFFFFF);
        return;
    }
    
    STUB_LOG("CreateFileA(\"%s\" -> \"%s\", access=0x%X)", filename, unix_path, dwDesiredAccess);

    int flags = 0;
    if ((dwDesiredAccess & 0x80000000) && (dwDesiredAccess & 0x40000000)) flags = O_RDWR;
    else if (dwDesiredAccess & 0x40000000) flags = O_WRONLY;
    else flags = O_RDONLY;

    switch (dwCreationDisposition) {
        case 1: flags |= O_CREAT | O_EXCL;  break;  /* CREATE_NEW */
        case 2: flags |= O_CREAT | O_TRUNC; break;  /* CREATE_ALWAYS */
        case 3: break;                              /* OPEN_EXISTING */
        case 4: flags |= O_CREAT;           break;  /* OPEN_ALWAYS */
        default: set_last_error(87); macwi_emu_reg_write(ctx, 0, 0xFFFFFFFF); return;
    }

    int fd = open(unix_path, flags, 0644);
    if (fd < 0) {
        set_last_error(2); /* ERROR_FILE_NOT_FOUND */
        macwi_emu_reg_write(ctx, 0, 0xFFFFFFFF);
    } else {
        extern HANDLE_TABLE g_macwi_handle_table;
        int* p_fd = malloc(sizeof(int));
        *p_fd = fd;
        HANDLE h = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_FILE, p_fd);
        macwi_emu_reg_write(ctx, 0, (uint32_t)h);
    }
}

static void win32_ReadFile(EMU_CONTEXT* ctx) {
    uint32_t hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped;
    macwi_thunk_read_param_32(ctx, 0, &hFile);
    macwi_thunk_read_param_32(ctx, 1, &lpBuffer);
    macwi_thunk_read_param_32(ctx, 2, &nNumberOfBytesToRead);
    macwi_thunk_read_param_32(ctx, 3, &lpNumberOfBytesRead);
    macwi_thunk_read_param_32(ctx, 4, &lpOverlapped);

    extern HANDLE_TABLE g_macwi_handle_table;
    int* p_fd = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hFile, HANDLE_TYPE_FILE, (void**)&p_fd) != MACWI_SUCCESS) {
        set_last_error(6); // ERROR_INVALID_HANDLE
        macwi_emu_reg_write(ctx, 0, 0); // FALSE
        return;
    }

    int fd = *p_fd;
    void* temp_buf = malloc(nNumberOfBytesToRead);
    
    ssize_t n = read(fd, temp_buf, nNumberOfBytesToRead);
    if (n < 0) {
        set_last_error(6);
        macwi_emu_reg_write(ctx, 0, 0); // FALSE
    } else {
        macwi_emu_write_memory(ctx, lpBuffer, temp_buf, n);
        if (lpNumberOfBytesRead != 0) {
            uint32_t n32 = (uint32_t)n;
            macwi_emu_write_memory(ctx, lpNumberOfBytesRead, &n32, 4);
        }
        macwi_emu_reg_write(ctx, 0, 1); // TRUE
    }
    free(temp_buf);
}

static void win32_WriteFile(EMU_CONTEXT* ctx) {
    uint32_t hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped;
    macwi_thunk_read_param_32(ctx, 0, &hFile);
    macwi_thunk_read_param_32(ctx, 1, &lpBuffer);
    macwi_thunk_read_param_32(ctx, 2, &nNumberOfBytesToWrite);
    macwi_thunk_read_param_32(ctx, 3, &lpNumberOfBytesWritten);
    macwi_thunk_read_param_32(ctx, 4, &lpOverlapped);

    extern HANDLE_TABLE g_macwi_handle_table;
    int* p_fd = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hFile, HANDLE_TYPE_FILE, (void**)&p_fd) != MACWI_SUCCESS) {
        set_last_error(6); // ERROR_INVALID_HANDLE
        macwi_emu_reg_write(ctx, 0, 0); // FALSE
        return;
    }

    int fd = *p_fd;
    void* temp_buf = malloc(nNumberOfBytesToWrite);
    macwi_emu_read_memory(ctx, lpBuffer, temp_buf, nNumberOfBytesToWrite);
    
    ssize_t n = write(fd, temp_buf, nNumberOfBytesToWrite);
    if (n < 0) {
        set_last_error(6);
        macwi_emu_reg_write(ctx, 0, 0); // FALSE
    } else {
        if (lpNumberOfBytesWritten != 0) {
            uint32_t n32 = (uint32_t)n;
            macwi_emu_write_memory(ctx, lpNumberOfBytesWritten, &n32, 4);
        }
        macwi_emu_reg_write(ctx, 0, 1); // TRUE
    }
    free(temp_buf);
}

static void win32_CloseHandle(EMU_CONTEXT* ctx) {
    uint32_t hObject;
    macwi_thunk_read_param_32(ctx, 0, &hObject);
    STUB_LOG("CloseHandle(0x%08X)", hObject);

    extern HANDLE_TABLE g_macwi_handle_table;
    
    // First, check if it's a file descriptor we need to close
    int* p_fd = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hObject, HANDLE_TYPE_FILE, (void**)&p_fd) == MACWI_SUCCESS) {
        close(*p_fd);
        free(p_fd);
    }
    
    // Then just close the handle
    macwi_status_t status = macwi_handle_close(&g_macwi_handle_table, (HANDLE)(uintptr_t)hObject);
    macwi_emu_reg_write(ctx, 0, status == MACWI_SUCCESS ? 1 : 0);
}

/* ============================================================================
 * Memory Management
 * ============================================================================ */

static void win32_VirtualAlloc(EMU_CONTEXT* ctx) {
    uint32_t lpAddress, dwSize, flAllocationType, flProtect;
    macwi_thunk_read_param_32(ctx, 0, &lpAddress);
    macwi_thunk_read_param_32(ctx, 1, &dwSize);
    macwi_thunk_read_param_32(ctx, 2, &flAllocationType);
    macwi_thunk_read_param_32(ctx, 3, &flProtect);

    STUB_LOG("VirtualAlloc(addr=0x%X, size=%u)", lpAddress, dwSize);
    
    // Win32 VirtualAlloc uses address=0 for OS-assigned allocation.
    // In our case, we can map to a specific free guest area. For now, we
    // allocate at an arbitrary high address if lpAddress is 0.
    static uint32_t next_alloc = 0x50000000;
    
    uint32_t alloc_addr = lpAddress;
    if (alloc_addr == 0) {
        alloc_addr = next_alloc;
        next_alloc += ((dwSize + 0xFFF) & ~0xFFF); // Page align
    }

    macwi_status_t st = macwi_emu_map_memory(ctx, alloc_addr, dwSize, MACWI_PROT_ALL);
    if (st == MACWI_SUCCESS) {
        macwi_emu_reg_write(ctx, 0, alloc_addr);
    } else {
        set_last_error(8); /* ERROR_NOT_ENOUGH_MEMORY */
        macwi_emu_reg_write(ctx, 0, 0);
    }
}

static void win32_VirtualFree(EMU_CONTEXT* ctx) {
    uint32_t lpAddress, dwSize, dwFreeType;
    macwi_thunk_read_param_32(ctx, 0, &lpAddress);
    macwi_thunk_read_param_32(ctx, 1, &dwSize);
    macwi_thunk_read_param_32(ctx, 2, &dwFreeType);

    STUB_LOG("VirtualFree(addr=0x%X)", lpAddress);
    
    macwi_emu_unmap_memory(ctx, lpAddress, dwSize); // We might need an unmap API in emu.h
    macwi_emu_reg_write(ctx, 0, 1); // TRUE
}

/* ============================================================================
 * ExitProcess
 * ============================================================================ */

static void win32_ExitProcess(EMU_CONTEXT* ctx) {
    uint32_t uExitCode;
    macwi_thunk_read_param_32(ctx, 0, &uExitCode);
    STUB_LOG("ExitProcess(%u)", uExitCode);
    exit(uExitCode);
}

/* ============================================================================
 * Threading & Synchronization APIs
 * ============================================================================ */

static void win32_CreateThread(EMU_CONTEXT* ctx) {
    uint32_t lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId;
    macwi_thunk_read_param_32(ctx, 0, &lpThreadAttributes);
    macwi_thunk_read_param_32(ctx, 1, &dwStackSize);
    macwi_thunk_read_param_32(ctx, 2, &lpStartAddress);
    macwi_thunk_read_param_32(ctx, 3, &lpParameter);
    macwi_thunk_read_param_32(ctx, 4, &dwCreationFlags);
    macwi_thunk_read_param_32(ctx, 5, &lpThreadId);

    STUB_LOG("CreateThread(start=0x%08X, param=0x%08X)", lpStartAddress, lpParameter);
    
    MacWIThread* th = NULL;
    if (macwi_thread_create(ctx, lpStartAddress, lpParameter, &th) == MACWI_SUCCESS) {
        extern HANDLE_TABLE g_macwi_handle_table;
        HANDLE h = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_THREAD, th);
        if (lpThreadId) {
            macwi_emu_write_memory(ctx, lpThreadId, &th->thread_id, 4);
        }
        macwi_emu_reg_write(ctx, 0, (uint32_t)(uintptr_t)h);
    } else {
        macwi_emu_reg_write(ctx, 0, 0); // NULL
    }
}

static void win32_CreateMutexA(EMU_CONTEXT* ctx) {
    uint32_t lpMutexAttributes, bInitialOwner, lpName;
    macwi_thunk_read_param_32(ctx, 0, &lpMutexAttributes);
    macwi_thunk_read_param_32(ctx, 1, &bInitialOwner);
    macwi_thunk_read_param_32(ctx, 2, &lpName);
    char name[256] = {0};
    if (lpName) macwi_thunk_read_guest_string(ctx, lpName, name, sizeof(name));

    STUB_LOG("CreateMutexA(name=\"%s\", initial=%d)", name, bInitialOwner);
    
    pthread_mutex_t* m = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(m, NULL);
    if (bInitialOwner) pthread_mutex_lock(m);

    extern HANDLE_TABLE g_macwi_handle_table;
    HANDLE h = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_MUTEX, m);
    macwi_emu_reg_write(ctx, 0, (uint32_t)(uintptr_t)h);
}

static void win32_WaitForSingleObject(EMU_CONTEXT* ctx) {
    uint32_t hHandle, dwMilliseconds;
    macwi_thunk_read_param_32(ctx, 0, &hHandle);
    macwi_thunk_read_param_32(ctx, 1, &dwMilliseconds);

    STUB_LOG("WaitForSingleObject(handle=0x%08X, ms=%u)", hHandle, dwMilliseconds);
    
    extern HANDLE_TABLE g_macwi_handle_table;
    pthread_mutex_t* m = NULL;
    MacWIThread* th = NULL;
    
    // Check if it's a Mutex
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hHandle, HANDLE_TYPE_MUTEX, (void**)&m) == MACWI_SUCCESS) {
        pthread_mutex_lock(m);
        macwi_emu_reg_write(ctx, 0, 0); // WAIT_OBJECT_0
        return;
    }
    
    // Check if it's a Thread
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hHandle, HANDLE_TYPE_THREAD, (void**)&th) == MACWI_SUCCESS) {
        pthread_join(th->host_thread, NULL);
        macwi_emu_reg_write(ctx, 0, 0); // WAIT_OBJECT_0
        return;
    }
    
    macwi_emu_reg_write(ctx, 0, 0xFFFFFFFF); // WAIT_FAILED
}

static void win32_ReleaseMutex(EMU_CONTEXT* ctx) {
    uint32_t hMutex;
    macwi_thunk_read_param_32(ctx, 0, &hMutex);

    STUB_LOG("ReleaseMutex(0x%08X)", hMutex);
    
    extern HANDLE_TABLE g_macwi_handle_table;
    pthread_mutex_t* m = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hMutex, HANDLE_TYPE_MUTEX, (void**)&m) == MACWI_SUCCESS) {
        pthread_mutex_unlock(m);
        macwi_emu_reg_write(ctx, 0, 1); // TRUE
    } else {
        macwi_emu_reg_write(ctx, 0, 0); // FALSE
    }
}

/* ============================================================================
 * API Registration
 * ============================================================================ */

static void win32_GetModuleHandleA(EMU_CONTEXT* ctx) {
    uint32_t lpModuleName;
    macwi_thunk_read_param_32(ctx, 0, &lpModuleName);
    
    // For now, always return the Image Base 0x00400000
    macwi_emu_reg_write(ctx, 0, 0x00400000);
}

void macwi_kernel32_register_apis(void) {
    macwi_thunk_register_api("kernel32.dll", "GetLastError", win32_GetLastError, 0);
    macwi_thunk_register_api("kernel32.dll", "SetLastError", win32_SetLastError, 1);
    macwi_thunk_register_api("kernel32.dll", "GetTickCount", win32_GetTickCount, 0);
    macwi_thunk_register_api("kernel32.dll", "Sleep", win32_Sleep, 1);
    macwi_thunk_register_api("kernel32.dll", "OutputDebugStringA", win32_OutputDebugStringA, 1);
    macwi_thunk_register_api("kernel32.dll", "lstrlenA", win32_lstrlenA, 1);
    macwi_thunk_register_api("kernel32.dll", "CreateFileA", win32_CreateFileA, 7);
    macwi_thunk_register_api("kernel32.dll", "ReadFile", win32_ReadFile, 5);
    macwi_thunk_register_api("kernel32.dll", "WriteFile", win32_WriteFile, 5);
    macwi_thunk_register_api("kernel32.dll", "CloseHandle", win32_CloseHandle, 1);
    macwi_thunk_register_api("kernel32.dll", "VirtualAlloc", win32_VirtualAlloc, 4);
    macwi_thunk_register_api("kernel32.dll", "VirtualFree", win32_VirtualFree, 3);
    macwi_thunk_register_api("kernel32.dll", "SetLastError", win32_SetLastError, 1);
    macwi_thunk_register_api("kernel32.dll", "GetModuleHandleA", win32_GetModuleHandleA, 1);
    macwi_thunk_register_api("kernel32.dll", "CreateThread", win32_CreateThread, 6);
    macwi_thunk_register_api("kernel32.dll", "CreateMutexA", win32_CreateMutexA, 3);
    macwi_thunk_register_api("kernel32.dll", "WaitForSingleObject", win32_WaitForSingleObject, 2);
    macwi_thunk_register_api("kernel32.dll", "ReleaseMutex", win32_ReleaseMutex, 1);
    macwi_thunk_register_api("kernel32.dll", "ExitProcess", win32_ExitProcess, 1);
}
