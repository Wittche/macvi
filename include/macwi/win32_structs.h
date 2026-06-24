/**
 * @file win32_structs.h
 * @brief 32-bit Win32 Structure Definitions for Marshaling
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#pragma pack(push, 1)

/* ============================================================================
 * user32.dll Structures
 * ============================================================================ */

typedef struct {
    uint32_t cbSize;
    uint32_t style;
    uint32_t lpfnWndProc; // Function pointer (32-bit)
    int32_t  cbClsExtra;
    int32_t  cbWndExtra;
    uint32_t hInstance;   // Handle
    uint32_t hIcon;       // Handle
    uint32_t hCursor;     // Handle
    uint32_t hbrBackground; // Handle
    uint32_t lpszMenuName;  // Pointer to string (32-bit)
    uint32_t lpszClassName; // Pointer to string (32-bit)
    uint32_t hIconSm;     // Handle
} WNDCLASSEXA_32;

typedef struct {
    int32_t x;
    int32_t y;
} POINT_32;

typedef struct {
    uint32_t hwnd;        // Handle
    uint32_t message;
    uint32_t wParam;
    uint32_t lParam;
    uint32_t time;
    POINT_32 pt;
} MSG_32;

/* ============================================================================
 * kernel32.dll Structures
 * ============================================================================ */

typedef struct {
    uint32_t dwOemId;
    uint32_t dwPageSize;
    uint32_t lpMinimumApplicationAddress; // Pointer
    uint32_t lpMaximumApplicationAddress; // Pointer
    uint32_t dwActiveProcessorMask;
    uint32_t dwNumberOfProcessors;
    uint32_t dwProcessorType;
    uint32_t dwAllocationGranularity;
    uint16_t wProcessorLevel;
    uint16_t wProcessorRevision;
} SYSTEM_INFO_32;

#pragma pack(pop)
