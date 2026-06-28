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
#include <sys/stat.h>

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

static void win32_DeleteFileA(EMU_CONTEXT* ctx) {
    uint64_t lpFileName;
    macwi_thunk_read_param_64(ctx, 0, &lpFileName);
    char filename[256];
    macwi_thunk_read_guest_string(ctx, lpFileName, filename, sizeof(filename));
    
    char unix_path[MACWI_MAX_PATH];
    macwi_vfs_dos_to_unix(filename, unix_path);
    STUB_LOG("DeleteFileA(\"%s\")", filename);
    
    if (unlink(unix_path) == 0) {
        macwi_emu_reg_write_64(ctx, 0, 1);
    } else {
        set_last_error(2); // ERROR_FILE_NOT_FOUND
        macwi_emu_reg_write_64(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_GetFileAttributesA(EMU_CONTEXT* ctx) {
    uint64_t lpFileName;
    macwi_thunk_read_param_64(ctx, 0, &lpFileName);
    char filename[256];
    macwi_thunk_read_guest_string(ctx, lpFileName, filename, sizeof(filename));
    
    char unix_path[MACWI_MAX_PATH];
    macwi_vfs_dos_to_unix(filename, unix_path);
    STUB_LOG("GetFileAttributesA(\"%s\")", filename);
    
    struct stat st;
    if (stat(unix_path, &st) == 0) {
        uint32_t attr = 0x80; // FILE_ATTRIBUTE_NORMAL
        if (S_ISDIR(st.st_mode)) attr = 0x10; // FILE_ATTRIBUTE_DIRECTORY
        macwi_emu_reg_write_64(ctx, 0, attr);
    } else {
        set_last_error(2); // ERROR_FILE_NOT_FOUND
        macwi_emu_reg_write_64(ctx, 0, 0xFFFFFFFF); // INVALID_FILE_ATTRIBUTES
    }
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_SetFileAttributesA(EMU_CONTEXT* ctx) {
    uint64_t lpFileName, dwFileAttributes;
    macwi_thunk_read_param_64(ctx, 0, &lpFileName);
    macwi_thunk_read_param_64(ctx, 1, &dwFileAttributes);
    STUB_LOG("SetFileAttributesA()");
    // Stub implementation
    macwi_emu_reg_write_64(ctx, 0, 1); // TRUE
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_GetSystemDirectoryA(EMU_CONTEXT* ctx) {
    uint64_t lpBuffer, uSize;
    macwi_thunk_read_param_64(ctx, 0, &lpBuffer);
    macwi_thunk_read_param_64(ctx, 1, &uSize);
    
    const char* sysdir = "C:\\Windows\\System32";
    size_t len = strlen(sysdir);
    
    STUB_LOG("GetSystemDirectoryA()");
    if (uSize > len) {
        macwi_emu_write_memory(ctx, lpBuffer, sysdir, len + 1);
        macwi_emu_reg_write_64(ctx, 0, len);
    } else {
        macwi_emu_reg_write_64(ctx, 0, len + 1);
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_GetWindowsDirectoryA(EMU_CONTEXT* ctx) {
    uint64_t lpBuffer, uSize;
    macwi_thunk_read_param_64(ctx, 0, &lpBuffer);
    macwi_thunk_read_param_64(ctx, 1, &uSize);
    
    const char* windir = "C:\\Windows";
    size_t len = strlen(windir);
    
    STUB_LOG("GetWindowsDirectoryA()");
    if (uSize > len) {
        macwi_emu_write_memory(ctx, lpBuffer, windir, len + 1);
        macwi_emu_reg_write_64(ctx, 0, len);
    } else {
        macwi_emu_reg_write_64(ctx, 0, len + 1);
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

// WIN32_FIND_DATAA structure
#pragma pack(push, 1)
struct win32_find_data_a {
    uint32_t dwFileAttributes;
    uint32_t ftCreationTime[2];
    uint32_t ftLastAccessTime[2];
    uint32_t ftLastWriteTime[2];
    uint32_t nFileSizeHigh;
    uint32_t nFileSizeLow;
    uint32_t dwReserved0;
    uint32_t dwReserved1;
    char cFileName[260];
    char cAlternateFileName[14];
};
#pragma pack(pop)

#include <dirent.h>

static void win32_FindFirstFileA(EMU_CONTEXT* ctx) {
    uint64_t lpFileName, lpFindFileData;
    macwi_thunk_read_param_64(ctx, 0, &lpFileName);
    macwi_thunk_read_param_64(ctx, 1, &lpFindFileData);
    
    char filename[256];
    macwi_thunk_read_guest_string(ctx, lpFileName, filename, sizeof(filename));
    STUB_LOG("FindFirstFileA(\"%s\")", filename);
    
    // We only support exact path or path + "*".
    // For simplicity, strip trailing "\*" or "/*" to get the directory path.
    char dirpath[256];
    strncpy(dirpath, filename, sizeof(dirpath));
    size_t len = strlen(dirpath);
    if (len >= 2 && dirpath[len-1] == '*' && (dirpath[len-2] == '\\' || dirpath[len-2] == '/')) {
        dirpath[len-2] = '\0';
    }
    
    char unix_path[MACWI_MAX_PATH];
    macwi_vfs_dos_to_unix(dirpath, unix_path);
    
    DIR* dir = opendir(unix_path);
    if (!dir) {
        set_last_error(2); // ERROR_FILE_NOT_FOUND
        macwi_emu_reg_write_64(ctx, 0, 0xFFFFFFFFFFFFFFFFULL); // INVALID_HANDLE_VALUE
        macwi_thunk_stdcall_return(ctx, 2);
        return;
    }
    
    struct dirent* ent = readdir(dir);
    if (!ent) {
        closedir(dir);
        set_last_error(2); // ERROR_FILE_NOT_FOUND
        macwi_emu_reg_write_64(ctx, 0, 0xFFFFFFFFFFFFFFFFULL); // INVALID_HANDLE_VALUE
        macwi_thunk_stdcall_return(ctx, 2);
        return;
    }
    
    struct win32_find_data_a fd = {0};
    fd.dwFileAttributes = (ent->d_type == DT_DIR) ? 0x10 : 0x80;
    strncpy(fd.cFileName, ent->d_name, sizeof(fd.cFileName) - 1);
    
    macwi_emu_write_memory(ctx, lpFindFileData, &fd, sizeof(fd));
    
    extern HANDLE_TABLE g_macwi_handle_table;
    HANDLE h = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_FIND_FILE, dir);
    macwi_emu_reg_write_64(ctx, 0, (uint64_t)h);
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_FindNextFileA(EMU_CONTEXT* ctx) {
    uint64_t hFindFile, lpFindFileData;
    macwi_thunk_read_param_64(ctx, 0, &hFindFile);
    macwi_thunk_read_param_64(ctx, 1, &lpFindFileData);
    STUB_LOG("FindNextFileA()");
    
    extern HANDLE_TABLE g_macwi_handle_table;
    DIR* dir = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hFindFile, HANDLE_TYPE_FIND_FILE, (void**)&dir) != MACWI_SUCCESS) {
        set_last_error(6); // ERROR_INVALID_HANDLE
        macwi_emu_reg_write_64(ctx, 0, 0); // FALSE
        macwi_thunk_stdcall_return(ctx, 2);
        return;
    }
    
    struct dirent* ent = readdir(dir);
    if (!ent) {
        set_last_error(18); // ERROR_NO_MORE_FILES
        macwi_emu_reg_write_64(ctx, 0, 0); // FALSE
        macwi_thunk_stdcall_return(ctx, 2);
        return;
    }
    
    struct win32_find_data_a fd = {0};
    fd.dwFileAttributes = (ent->d_type == DT_DIR) ? 0x10 : 0x80;
    strncpy(fd.cFileName, ent->d_name, sizeof(fd.cFileName) - 1);
    
    macwi_emu_write_memory(ctx, lpFindFileData, &fd, sizeof(fd));
    
    macwi_emu_reg_write_64(ctx, 0, 1); // TRUE
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_FindClose(EMU_CONTEXT* ctx) {
    uint64_t hFindFile;
    macwi_thunk_read_param_64(ctx, 0, &hFindFile);
    STUB_LOG("FindClose()");
    
    extern HANDLE_TABLE g_macwi_handle_table;
    DIR* dir = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hFindFile, HANDLE_TYPE_FIND_FILE, (void**)&dir) == MACWI_SUCCESS) {
        closedir(dir);
    }
    macwi_status_t status = macwi_handle_close(&g_macwi_handle_table, (HANDLE)(uintptr_t)hFindFile);
    macwi_emu_reg_write_64(ctx, 0, status == MACWI_SUCCESS ? 1 : 0);
    macwi_thunk_stdcall_return(ctx, 1);
}

/* ============================================================================
 * Console I/O
 * ============================================================================ */

static void win32_GetStdHandle(EMU_CONTEXT* ctx) {
    uint64_t nStdHandle;
    macwi_thunk_read_param_64(ctx, 0, &nStdHandle);
    uint32_t n32 = (uint32_t)nStdHandle;
    
    STUB_LOG("GetStdHandle(%d)", (int)n32);
    
    // STD_INPUT_HANDLE = -10 (0xFFFFFFF6)
    // STD_OUTPUT_HANDLE = -11 (0xFFFFFFF5)
    // STD_ERROR_HANDLE = -12 (0xFFFFFFF4)
    
    if (n32 == 0xFFFFFFF6 || n32 == 0xFFFFFFF5 || n32 == 0xFFFFFFF4) {
        macwi_emu_reg_write_64(ctx, 0, (uint64_t)n32);
    } else {
        macwi_emu_reg_write_64(ctx, 0, 0xFFFFFFFFFFFFFFFFULL); // INVALID_HANDLE_VALUE
    }
    
    macwi_thunk_stdcall_return(ctx, 1);
}

static void win32_ReadFile(EMU_CONTEXT* ctx) {
    uint64_t hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped;
    macwi_thunk_read_param_64(ctx, 0, &hFile);
    macwi_thunk_read_param_64(ctx, 1, &lpBuffer);
    macwi_thunk_read_param_64(ctx, 2, &nNumberOfBytesToRead);
    macwi_thunk_read_param_64(ctx, 3, &lpNumberOfBytesRead);
    macwi_thunk_read_param_64(ctx, 4, &lpOverlapped);

    STUB_LOG("ReadFile(handle=0x%X, bytes=%u)", (uint32_t)hFile, (uint32_t)nNumberOfBytesToRead);

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

    STUB_LOG("WriteFile(handle=0x%X, bytes=%u)", (uint32_t)hFile, (uint32_t)nNumberOfBytesToWrite);

    uint32_t h32 = (uint32_t)hFile;
    int fd = -1;

    // Check pseudo-handles for console
    if (h32 == 0xFFFFFFF5) { // STD_OUTPUT_HANDLE
        fd = STDOUT_FILENO;
    } else if (h32 == 0xFFFFFFF4) { // STD_ERROR_HANDLE
        fd = STDERR_FILENO;
    } else {
        extern HANDLE_TABLE g_macwi_handle_table;
        int* p_fd = NULL;
        if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hFile, HANDLE_TYPE_FILE, (void**)&p_fd) != MACWI_SUCCESS) {
            set_last_error(6); // ERROR_INVALID_HANDLE
            macwi_emu_reg_write_64(ctx, 0, 0); // FALSE
            macwi_thunk_stdcall_return(ctx, 5);
            return;
        }
        fd = *p_fd;
    }

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
    
    // We can peek the handle type without error if we just try to close it.
    // But we need to free the inner object depending on type.
    // For now, we will just use a helper or do it manually if it's a file.
    // Actually, macwi_handle_close just frees the entry. We must free the underlying resource first.
    // Let's try to get it as FILE, if it works, close(fd). If MUTEX, pthread_mutex_destroy.
    int* p_fd = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hObject, HANDLE_TYPE_FILE, (void**)&p_fd) == MACWI_SUCCESS) {
        close(*p_fd);
        free(p_fd);
    } else {
        void* m_obj = NULL;
        if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hObject, HANDLE_TYPE_MUTEX, &m_obj) == MACWI_SUCCESS) {
            pthread_mutex_destroy((pthread_mutex_t*)m_obj);
            free(m_obj);
        }
        // threads don't have allocated memory in the object (it's just tid)
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
    
    // Check if it's MEM_RELEASE or MEM_DECOMMIT
    // dwFreeType MEM_RELEASE = 0x8000, MEM_DECOMMIT = 0x4000
    if (dwFreeType == 0x8000) {
        macwi_emu_unmap_memory(ctx, lpAddress, dwSize);
    }
    
    macwi_emu_reg_write_64(ctx, 0, 1); // TRUE
    macwi_thunk_stdcall_return(ctx, 3);
}

static void win32_GetProcessHeap(EMU_CONTEXT* ctx) {
    STUB_LOG("GetProcessHeap()");
    // Return a dummy heap handle
    macwi_emu_reg_write_64(ctx, 0, 0x10000000); 
    macwi_thunk_stdcall_return(ctx, 0);
}

static void win32_HeapCreate(EMU_CONTEXT* ctx) {
    uint64_t flOptions, dwInitialSize, dwMaximumSize;
    macwi_thunk_read_param_64(ctx, 0, &flOptions);
    macwi_thunk_read_param_64(ctx, 1, &dwInitialSize);
    macwi_thunk_read_param_64(ctx, 2, &dwMaximumSize);

    STUB_LOG("HeapCreate(opt=0x%X, init=%u, max=%u)", (uint32_t)flOptions, (uint32_t)dwInitialSize, (uint32_t)dwMaximumSize);
    // Return a dummy heap handle
    static uint32_t next_heap_handle = 0x20000000;
    macwi_emu_reg_write_64(ctx, 0, next_heap_handle);
    next_heap_handle += 0x1000;
    macwi_thunk_stdcall_return(ctx, 3);
}

static void win32_HeapAlloc(EMU_CONTEXT* ctx) {
    uint64_t hHeap, dwFlags, dwBytes;
    macwi_thunk_read_param_64(ctx, 0, &hHeap);
    macwi_thunk_read_param_64(ctx, 1, &dwFlags);
    macwi_thunk_read_param_64(ctx, 2, &dwBytes);

    STUB_LOG("HeapAlloc(heap=0x%X, flags=0x%X, bytes=%u)", (uint32_t)hHeap, (uint32_t)dwFlags, (uint32_t)dwBytes);
    
    // Very simple linear allocator for now, just page-aligned VirtualAlloc equivalent
    static uint64_t next_alloc = 0x60000000ULL;
    uint64_t alloc_addr = next_alloc;
    uint32_t size = (dwBytes + 0xFFF) & ~0xFFF; // Page align
    next_alloc += size;

    macwi_status_t st = macwi_emu_map_memory(ctx, alloc_addr, size, MACWI_PROT_ALL, NULL);
    if (st == MACWI_SUCCESS) {
        macwi_emu_reg_write_64(ctx, 0, alloc_addr);
        if (dwFlags & 0x00000008) { // HEAP_ZERO_MEMORY
            // Memory mapped via mmap anonymous is already zeroed
        }
    } else {
        set_last_error(8); // ERROR_NOT_ENOUGH_MEMORY
        macwi_emu_reg_write_64(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 3);
}

static void win32_HeapFree(EMU_CONTEXT* ctx) {
    uint64_t hHeap, dwFlags, lpMem;
    macwi_thunk_read_param_64(ctx, 0, &hHeap);
    macwi_thunk_read_param_64(ctx, 1, &dwFlags);
    macwi_thunk_read_param_64(ctx, 2, &lpMem);

    STUB_LOG("HeapFree(heap=0x%X, mem=0x%llX)", (uint32_t)hHeap, lpMem);
    // Real implementation would unmap or add to free list.
    // For our simple linear allocator, we just ignore it (leak memory).
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
 * Threading and Synchronization
 * ============================================================================ */

static void win32_CreateThread(EMU_CONTEXT* ctx) {
    uint64_t lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId;
    macwi_thunk_read_param_64(ctx, 0, &lpThreadAttributes);
    macwi_thunk_read_param_64(ctx, 1, &dwStackSize);
    macwi_thunk_read_param_64(ctx, 2, &lpStartAddress);
    macwi_thunk_read_param_64(ctx, 3, &lpParameter);
    macwi_thunk_read_param_64(ctx, 4, &dwCreationFlags);
    macwi_thunk_read_param_64(ctx, 5, &lpThreadId);

    STUB_LOG("CreateThread(start=0x%llX, param=0x%llX)", lpStartAddress, lpParameter);
    
    uint64_t thread_id = 0;
    macwi_status_t st = macwi_emu_create_thread(ctx, lpStartAddress, lpParameter, dwStackSize, &thread_id);
    if (st == MACWI_SUCCESS) {
        if (lpThreadId) {
            uint32_t tid32 = (uint32_t)thread_id;
            macwi_emu_write_memory(ctx, lpThreadId, &tid32, 4);
        }
        
        extern HANDLE_TABLE g_macwi_handle_table;
        HANDLE h = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_THREAD, (void*)(uintptr_t)thread_id);
        macwi_emu_reg_write_64(ctx, 0, (uint64_t)h);
    } else {
        set_last_error(8); // ERROR_NOT_ENOUGH_MEMORY
        macwi_emu_reg_write_64(ctx, 0, 0);
    }
    macwi_thunk_stdcall_return(ctx, 6);
}

static void win32_GetCurrentThreadId(EMU_CONTEXT* ctx) {
    STUB_LOG("GetCurrentThreadId()");
    macwi_emu_reg_write_64(ctx, 0, (uint64_t)pthread_self());
    macwi_thunk_stdcall_return(ctx, 0);
}

static void win32_ExitThread(EMU_CONTEXT* ctx) {
    uint64_t dwExitCode;
    macwi_thunk_read_param_64(ctx, 0, &dwExitCode);
    STUB_LOG("ExitThread(%u)", (uint32_t)dwExitCode);
    // Actually stopping the thread from within is tricky via FEX_ThreadExecute return,
    // but we can exit the pthread.
    pthread_exit((void*)(uintptr_t)dwExitCode);
}

static void win32_CreateMutexA(EMU_CONTEXT* ctx) {
    uint64_t lpMutexAttributes, bInitialOwner, lpName;
    macwi_thunk_read_param_64(ctx, 0, &lpMutexAttributes);
    macwi_thunk_read_param_64(ctx, 1, &bInitialOwner);
    macwi_thunk_read_param_64(ctx, 2, &lpName);

    STUB_LOG("CreateMutexA()");
    pthread_mutex_t* m = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(m, NULL);
    if (bInitialOwner) {
        pthread_mutex_lock(m);
    }
    
    extern HANDLE_TABLE g_macwi_handle_table;
    HANDLE h = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_MUTEX, m);
    macwi_emu_reg_write_64(ctx, 0, (uint64_t)h);
    macwi_thunk_stdcall_return(ctx, 3);
}

static void win32_WaitForSingleObject(EMU_CONTEXT* ctx) {
    uint64_t hHandle, dwMilliseconds;
    macwi_thunk_read_param_64(ctx, 0, &hHandle);
    macwi_thunk_read_param_64(ctx, 1, &dwMilliseconds);

    STUB_LOG("WaitForSingleObject(handle=0x%X, ms=%u)", (uint32_t)hHandle, (uint32_t)dwMilliseconds);
    
    extern HANDLE_TABLE g_macwi_handle_table;
    void* obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hHandle, HANDLE_TYPE_MUTEX, &obj) == MACWI_SUCCESS) {
        pthread_mutex_t* m = (pthread_mutex_t*)obj;
        pthread_mutex_lock(m);
        macwi_emu_reg_write_64(ctx, 0, 0); // WAIT_OBJECT_0
    } else if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hHandle, HANDLE_TYPE_THREAD, &obj) == MACWI_SUCCESS) {
        pthread_t tid = (pthread_t)(uintptr_t)obj;
        pthread_join(tid, NULL);
        macwi_emu_reg_write_64(ctx, 0, 0); // WAIT_OBJECT_0
    } else {
        set_last_error(6); // ERROR_INVALID_HANDLE
        macwi_emu_reg_write_64(ctx, 0, 0xFFFFFFFF); // WAIT_FAILED
    }
    macwi_thunk_stdcall_return(ctx, 2);
}

static void win32_ReleaseMutex(EMU_CONTEXT* ctx) {
    uint64_t hMutex;
    macwi_thunk_read_param_64(ctx, 0, &hMutex);

    STUB_LOG("ReleaseMutex(handle=0x%X)", (uint32_t)hMutex);
    
    extern HANDLE_TABLE g_macwi_handle_table;
    void* obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hMutex, HANDLE_TYPE_MUTEX, &obj) == MACWI_SUCCESS) {
        pthread_mutex_t* m = (pthread_mutex_t*)obj;
        pthread_mutex_unlock(m);
        macwi_emu_reg_write_64(ctx, 0, 1); // TRUE
    } else {
        set_last_error(6); // ERROR_INVALID_HANDLE
        macwi_emu_reg_write_64(ctx, 0, 0); // FALSE
    }
    macwi_thunk_stdcall_return(ctx, 1);
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
    macwi_thunk_register_api("kernel32.dll", "GetStdHandle",       win32_GetStdHandle, 1);
    macwi_thunk_register_api("kernel32.dll", "CreateFileA",        win32_CreateFileA, 7);
    macwi_thunk_register_api("kernel32.dll", "DeleteFileA",        win32_DeleteFileA, 1);
    macwi_thunk_register_api("kernel32.dll", "GetFileAttributesA", win32_GetFileAttributesA, 1);
    macwi_thunk_register_api("kernel32.dll", "SetFileAttributesA", win32_SetFileAttributesA, 2);
    macwi_thunk_register_api("kernel32.dll", "GetSystemDirectoryA",win32_GetSystemDirectoryA, 2);
    macwi_thunk_register_api("kernel32.dll", "GetWindowsDirectoryA",win32_GetWindowsDirectoryA, 2);
    macwi_thunk_register_api("kernel32.dll", "FindFirstFileA",     win32_FindFirstFileA, 2);
    macwi_thunk_register_api("kernel32.dll", "FindNextFileA",      win32_FindNextFileA, 2);
    macwi_thunk_register_api("kernel32.dll", "FindClose",          win32_FindClose, 1);
    macwi_thunk_register_api("kernel32.dll", "ReadFile",           win32_ReadFile, 5);
    macwi_thunk_register_api("kernel32.dll", "WriteFile",          win32_WriteFile, 5);
    macwi_thunk_register_api("kernel32.dll", "CloseHandle",        win32_CloseHandle, 1);
    macwi_thunk_register_api("kernel32.dll", "VirtualAlloc",       win32_VirtualAlloc, 4);
    macwi_thunk_register_api("kernel32.dll", "VirtualFree",        win32_VirtualFree, 3);
    macwi_thunk_register_api("kernel32.dll", "GetProcessHeap",     win32_GetProcessHeap, 0);
    macwi_thunk_register_api("kernel32.dll", "HeapCreate",         win32_HeapCreate, 3);
    macwi_thunk_register_api("kernel32.dll", "HeapAlloc",          win32_HeapAlloc, 3);
    macwi_thunk_register_api("kernel32.dll", "HeapFree",           win32_HeapFree, 3);
    macwi_thunk_register_api("kernel32.dll", "CreateThread",       win32_CreateThread, 6);
    macwi_thunk_register_api("kernel32.dll", "GetCurrentThreadId", win32_GetCurrentThreadId, 0);
    macwi_thunk_register_api("kernel32.dll", "ExitThread",         win32_ExitThread, 1);
    macwi_thunk_register_api("kernel32.dll", "CreateMutexA",       win32_CreateMutexA, 3);
    macwi_thunk_register_api("kernel32.dll", "WaitForSingleObject",win32_WaitForSingleObject, 2);
    macwi_thunk_register_api("kernel32.dll", "ReleaseMutex",       win32_ReleaseMutex, 1);
    macwi_thunk_register_api("kernel32.dll", "ExitProcess",        win32_ExitProcess, 1);
}
