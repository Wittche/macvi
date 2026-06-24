/**
 * @file types.h
 * @brief Core type definitions for Win32 compatibility.
 *
 * Provides fixed-width integer types, pointer types, and status codes that
 * mirror the Windows SDK type system. These are used throughout MacWI to
 * faithfully represent Win32 data structures on the ARM64/macOS host.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * Fixed-width integer types — Win32 naming conventions
 * ============================================================================ */

typedef uint32_t DWORD;     /**< 32-bit unsigned integer (Double WORD) */
typedef uint16_t WORD;      /**< 16-bit unsigned integer                */
typedef uint8_t  BYTE;      /**< 8-bit unsigned integer                 */
typedef int32_t  LONG;      /**< 32-bit signed integer                  */
typedef uint32_t ULONG;     /**< 32-bit unsigned integer                */
typedef int16_t  SHORT;     /**< 16-bit signed integer                  */
typedef uint16_t USHORT;    /**< 16-bit unsigned integer                */
typedef int64_t  LONGLONG;  /**< 64-bit signed integer                  */
typedef uint64_t ULONGLONG; /**< 64-bit unsigned integer                */
typedef int32_t  INT;       /**< 32-bit signed integer (Win32 INT)      */
typedef uint32_t UINT;      /**< 32-bit unsigned integer (Win32 UINT)   */
typedef char     CHAR;      /**< 8-bit character (ANSI)                 */
typedef uint16_t WCHAR;     /**< 16-bit character (UTF-16LE)            */

/* ============================================================================
 * Pointer-width types
 *
 * On the host (ARM64 macOS) pointers are 64-bit.  WIN32_PTR32 represents a
 * 32-bit guest pointer that must be translated before dereferencing.
 * ============================================================================ */

typedef uint32_t WIN32_PTR32;  /**< Guest 32-bit pointer (address in PE32 space) */
typedef uint64_t WIN32_PTR64;  /**< Host 64-bit pointer stored as integer        */

typedef uint64_t SIZE_T;       /**< Host size_t equivalent (always 64-bit here)  */
typedef uint64_t ULONG_PTR;    /**< Unsigned integer wide enough for a pointer   */
typedef int64_t  LONG_PTR;     /**< Signed integer wide enough for a pointer     */

/* ============================================================================
 * Boolean type — Win32 uses a 32-bit BOOL, not C99 _Bool.
 * ============================================================================ */

typedef int32_t BOOL;

#ifndef TRUE
#define TRUE  ((BOOL)1)
#endif

#ifndef FALSE
#define FALSE ((BOOL)0)
#endif

/* ============================================================================
 * Handle and pointer types
 * ============================================================================ */

/** Opaque handle type — the actual value is an index into a handle table. */
typedef void* HANDLE;

typedef HANDLE HMODULE;    /**< Module (DLL/EXE) handle   */
typedef HANDLE HINSTANCE;  /**< Instance handle (== HMODULE in Win32) */

typedef void*        LPVOID;   /**< Pointer to any type        */
typedef const void*  LPCVOID;  /**< Pointer to const any type  */
typedef char*        LPSTR;    /**< Pointer to ANSI string     */
typedef const char*  LPCSTR;   /**< Pointer to const ANSI str  */
typedef WCHAR*       LPWSTR;   /**< Pointer to wide string     */
typedef const WCHAR* LPCWSTR;  /**< Pointer to const wide str  */

/* ============================================================================
 * Special handle constants
 * ============================================================================ */

/** Sentinel value indicating an invalid or uninitialized handle. */
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

/* ============================================================================
 * macwi_status_t — unified error codes for all MacWI subsystems
 *
 * Every fallible public function returns one of these values.  Callers should
 * compare against MACWI_SUCCESS and treat anything else as an error, though
 * the specific code carries diagnostic information.
 * ============================================================================ */

typedef enum macwi_status {
    MACWI_SUCCESS            = 0,   /**< Operation completed successfully        */
    MACWI_ERROR_INVALID_PE   = -1,  /**< PE header validation failed             */
    MACWI_ERROR_MEMORY       = -2,  /**< Memory allocation or mapping failure    */
    MACWI_ERROR_NOT_FOUND    = -3,  /**< Requested item (file, export, …) absent */
    MACWI_ERROR_UNSUPPORTED  = -4,  /**< Feature or format not yet implemented   */
    MACWI_ERROR_INVALID_PARAM = -5, /**< NULL pointer or out-of-range argument   */
    MACWI_ERROR_IO           = -6,  /**< File I/O or system call failure         */
} macwi_status_t;
