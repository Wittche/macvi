/**
 * @file kernel32.c
 * @brief Win32 kernel32.dll stub implementations for PE32+ (64-bit) integration.
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
    macwi_emu_reg_write_64(ctx, 0 /* RAX */, tls_last_error);
    macwi_thunk_stdcall_return(ctx, 0);
}

static void win32_SetLastError(EMU_CONTEXT* ctx) {
    uint64_t err;
    macwi_thunk_read_param_64(ctx, 0, &err);
    set_last_error((uint32_t)err);
    macwi_emu_reg_write_64(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

/* ============================================================================
 * Debugging and timing
 * ============================================================================ */

static void win32_GetTickCount(EMU_CONTEXT* ctx) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t ticks = (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    macwi_emu_reg_write_64(ctx, 0, ticks);
    macwi_thunk_stdcall_return(ctx, 0);
}

static void win32_Sleep(EMU_CONTEXT* ctx) {
    uint64_t ms;
    macwi_thunk_read_param_64(ctx, 0, &ms);
    STUB_LOG("Sleep(%u ms)", (uint32_t)ms);
    usleep((uint32_t)ms * 1000);
    macwi_emu_reg_write_64(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_OutputDebugStringA(EMU_CONTEXT* ctx) {
    uint64_t lpOutputString;
    macwi_thunk_read_param_64(ctx, 0, &lpOutputString);
    char buf[1024];
    macwi_thunk_read_guest_string(ctx, lpOutputString, buf, sizeof(buf));
    STUB_LOG("OutputDebugStringA: %s", buf);
    macwi_emu_reg_write_64(ctx, 0, 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_lstrlenA(EMU_CONTEXT* ctx) {
    uint64_t lpString;
    macwi_thunk_read_param_64(ctx, 0, &lpString);
    if (!lpString) {
        macwi_emu_reg_write_64(ctx, 0, 0);
        macwi_thunk_stdcall_return(ctx, 1);
        return;
    }
    char buf[4096];
    macwi_thunk_read_guest_string(ctx, lpString, buf, sizeof(buf));
    macwi_emu_reg_write_64(ctx, 0, strlen(buf));
    macwi_thunk_stdcall_return(ctx, 1);
}

/* ============================================================================
 * File I/O
 * ============================================================================ */

static void win32_CreateFileA(EMU_CONTEXT* ctx) {
    uint64_t lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes;
    uint64_t dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile;
    
    macwi_thunk_read_param_64(ctx, 0, &lpFileName);
    macwi_thunk_read_param_64(ctx, 1, &dwDesiredAccess);
    macwi_thunk_read_param_64(ctx, 2, &dwShareMode);
    macwi_thunk_read_param_64(ctx, 3, &lpSecurityAttributes);
    macwi_thunk_read_param_64(ctx, 4, &dwCreationDisposition);
    macwi_thunk_read_param_64(ctx, 5, &dwFlagsAndAttributes);
    macwi_thunk_read_param_64(ctx, 6, &hTemplateFile);

    char filename[256];
    macwi_thunk_read_guest_string(ctx, lpFileName, filename, sizeof(filename));
    
    char unix_path[MACWI_MAX_PATH];
    if (macwi_vfs_dos_to_unix(filename, unix_path) != MACWI_SUCCESS) {
        set_last_error(3); // ERROR_PATH_NOT_FOUND
        macwi_emu_reg_write_64(ctx, 0, 0xFFFFFFFFFFFFFFFFULL);
        macwi_thunk_stdcall_return(ctx, 7);
        return;
    }
    
    STUB_LOG("CreateFileA(\"%s\" -> \"%s\", access=0x%X)", filename, unix_path, (uint32_t)dwDesiredAccess);

    int flags = 0;
    if ((dwDesiredAccess & 0x80000000) && (dwDesiredAccess & 0x40000000)) flags = O_RDWR;
    else if (dwDesiredAccess & 0x40000000) flags = O_WRONLY;
    else flags = O_RDONLY;

    switch (dwCreationDisposition) {
        case 1: flags |= O_CREAT | O_EXCL;  break;  /* CREATE_NEW */
        case 2: flags |= O_CREAT | O_TRUNC; break;  /* CREATE_ALWAYS */
        case 3: break;                              /* OPEN_EXISTING */
        case 4: flags |= O_CREAT;           break;  /* OPEN_ALWAYS */
        default: set_last_error(87); macwi_emu_reg_write_64(ctx, 0, 0xFFFFFFFFFFFFFFFFULL); macwi_thunk_stdcall_return(ctx, 7); return;
    }

    int fd = open(unix_path, flags, 0644);
    if (fd < 0) {
        set_last_error(2); /* ERROR_FILE_NOT_FOUND */
        macwi_emu_reg_write_64(ctx, 0, 0xFFFFFFFFFFFFFFFFULL);
    } else {
        extern HANDLE_TABLE g_macwi_handle_table;
        int* p_fd = malloc(sizeof(int));
        *p_fd = fd;
        HANDLE h = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_FILE, p_fd);
        macwi_emu_reg_write_64(ctx, 0, (uint64_t)h);
    }
    macwi_thunk_stdcall_return(ctx, 7);
}

static void win32_ReadFile(EMU_CONTEXT* ctx) {
    uint64_t hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped;
    macwi_thunk_read_param_64(ctx, 0, &hFile);
    macwi_thunk_read_param_64(ctx, 1, &lpBuffer);
    macwi_thunk_read_param_64(ctx, 2, &nNumberOfBytesToRead);
    macwi_thunk_read_param_64(ctx, 3, &lpNumberOfBytesRead);
    macwi_thunk_read_param_64(ctx, 4, &lpOverlapped);

    extern HANDLE_TABLE g_macwi_handle_table;
    int* p_fd = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hFile, HANDLE_TYPE_FILE, (void**)&p_fd) != MACWI_SUCCESS) {
        set_last_error(6); // ERROR_INVALID_HANDLE
        macwi_emu_reg_write_64(ctx, 0, 0); // FALSE
        macwi_thunk_stdcall_return(ctx, 5);
        return;
    }

    int fd = *p_fd;
    void* temp_buf = malloc(nNumberOfBytesToRead);
    
    ssize_t n = read(fd, temp_buf, nNumberOfBytesToRead);
    if (n < 0) {
        set_last_error(6);
        macwi_emu_reg_write_64(ctx, 0, 0); // FALSE
    } else {
        macwi_emu_write_memory(ctx, lpBuffer, temp_buf, n);
        if (lpNumberOfBytesRead != 0) {
            uint32_t n32 = (uint32_t)n;
            macwi_emu_write_memory(ctx, lpNumberOfBytesRead, &n32, 4);
        }
        macwi_emu_reg_write_64(ctx, 0, 1); // TRUE
    }
    free(temp_buf);
    macwi_thunk_stdcall_return(ctx, 5);
}

static void win32_WriteFile(EMU_CONTEXT* ctx) {
    uint64_t hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped;
    macwi_thunk_read_param_64(ctx, 0, &hFile);
    macwi_thunk_read_param_64(ctx, 1, &lpBuffer);
    macwi_thunk_read_param_64(ctx, 2, &nNumberOfBytesToWrite);
    macwi_thunk_read_param_64(ctx, 3, &lpNumberOfBytesWritten);
    macwi_thunk_read_param_64(ctx, 4, &lpOverlapped);

    extern HANDLE_TABLE g_macwi_handle_table;
    int* p_fd = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hFile, HANDLE_TYPE_FILE, (void**)&p_fd) != MACWI_SUCCESS) {
        set_last_error(6); // ERROR_INVALID_HANDLE
        macwi_emu_reg_write_64(ctx, 0, 0); // FALSE
        macwi_thunk_stdcall_return(ctx, 5);
        return;
    }

    int fd = *p_fd;
    void* temp_buf = malloc(nNumberOfBytesToWrite);
    macwi_emu_read_memory(ctx, lpBuffer, temp_buf, nNumberOfBytesToWrite);
    
    ssize_t n = write(fd, temp_buf, nNumberOfBytesToWrite);
    if (n < 0) {
        set_last_error(6);
        macwi_emu_reg_write_64(ctx, 0, 0); // FALSE
    } else {
        if (lpNumberOfBytesWritten != 0) {
            uint32_t n32 = (uint32_t)n;
            macwi_emu_write_memory(ctx, lpNumberOfBytesWritten, &n32, 4);
        }
        macwi_emu_reg_write_64(ctx, 0, 1); // TRUE
    }
    free(temp_buf);
    macwi_thunk_stdcall_return(ctx, 5);
}

static void win32_CloseHandle(EMU_CONTEXT* ctx) {
    uint64_t hObject;
    macwi_thunk_read_param_64(ctx, 0, &hObject);
    STUB_LOG("CloseHandle(0x%llX)", hObject);

    extern HANDLE_TABLE g_macwi_handle_table;
    
    int* p_fd = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hObject, HANDLE_TYPE_FILE, (void**)&p_fd) == MACWI_SUCCESS) {
        close(*p_fd);
        free(p_fd);
    }
    
    macwi_status_t status = macwi_handle_close(&g_macwi_handle_table, (HANDLE)(uintptr_t)hObject);
    macwi_emu_reg_write_64(ctx, 0, status == MACWI_SUCCESS ? 1 : 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

/* ============================================================================
 * Memory Management
 * ============================================================================ */

static void win32_VirtualAlloc(EMU_CONTEXT* ctx) {
    uint64_t lpAddress, dwSize, flAllocationType, flProtect;
    macwi_thunk_read_param_64(ctx, 0, &lpAddress);
    macwi_thunk_read_param_64(ctx, 1, &dwSize);
    macwi_thunk_read_param_64(ctx, 2, &flAllocationType);
    macwi_thunk_read_param_64(ctx, 3, &flProtect);

    STUB_LOG("VirtualAlloc(addr=0x%llX, size=%u)", lpAddress, (uint32_t)dwSize);
    
    static uint64_t next_alloc = 0x50000000ULL; // High enough
    
    uint64_t alloc_addr = lpAddress;
    if (alloc_addr == 0) {
        alloc_addr = next_alloc;
        next_alloc += ((dwSize + 0xFFF) & ~0xFFF); // Page align
    }

    macwi_status_t st = macwi_emu_map_memory(ctx, alloc_addr, dwSize, MACWI_PROT_ALL, NULL);
    if (st == MACWI_SUCCESS) {
        macwi_emu_reg_write_64(ctx, 0, alloc_addr);
    } else {
        set_last_error(8); /* ERROR_NOT_ENOUGH_MEMORY */
        macwi_emu_reg_write_64(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 4);
}

static void win32_VirtualFree(EMU_CONTEXT* ctx) {
    uint64_t lpAddress, dwSize, dwFreeType;
    macwi_thunk_read_param_64(ctx, 0, &lpAddress);
    macwi_thunk_read_param_64(ctx, 1, &dwSize);
    macwi_thunk_read_param_64(ctx, 2, &dwFreeType);

    STUB_LOG("VirtualFree(addr=0x%llX)", lpAddress);
    
    // We don't have an unmap in our emu.h yet
    macwi_emu_reg_write_64(ctx, 0, 1); // TRUE
    macwi_thunk_stdcall_return(ctx, 3);
}

/* ============================================================================
 * ExitProcess
 * ============================================================================ */

static void win32_ExitProcess(EMU_CONTEXT* ctx) {
    uint64_t uExitCode;
    macwi_thunk_read_param_64(ctx, 0, &uExitCode);
    STUB_LOG("ExitProcess(%u)", (uint32_t)uExitCode);
    exit((int)uExitCode);
}

/* ============================================================================
 * Registration
 * ============================================================================ */

void macwi_kernel32_register_apis(void) {
    macwi_thunk_register_api("kernel32.dll", "GetLastError",       win32_GetLastError, 0);
    macwi_thunk_register_api("kernel32.dll", "SetLastError",       win32_SetLastError, 1);
    macwi_thunk_register_api("kernel32.dll", "GetTickCount",       win32_GetTickCount, 0);
    macwi_thunk_register_api("kernel32.dll", "Sleep",              win32_Sleep, 1);
    macwi_thunk_register_api("kernel32.dll", "OutputDebugStringA", win32_OutputDebugStringA, 1);
    macwi_thunk_register_api("kernel32.dll", "lstrlenA",           win32_lstrlenA, 1);
    macwi_thunk_register_api("kernel32.dll", "CreateFileA",        win32_CreateFileA, 7);
    macwi_thunk_register_api("kernel32.dll", "ReadFile",           win32_ReadFile, 5);
    macwi_thunk_register_api("kernel32.dll", "WriteFile",          win32_WriteFile, 5);
    macwi_thunk_register_api("kernel32.dll", "CloseHandle",        win32_CloseHandle, 1);
    macwi_thunk_register_api("kernel32.dll", "VirtualAlloc",       win32_VirtualAlloc, 4);
    macwi_thunk_register_api("kernel32.dll", "VirtualFree",        win32_VirtualFree, 3);
    macwi_thunk_register_api("kernel32.dll", "ExitProcess",        win32_ExitProcess, 1);
}
