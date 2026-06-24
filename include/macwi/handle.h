/**
 * @file handle.h
 * @brief Handle table management — Win32-style opaque handle system.
 *
 * Windows identifies kernel objects (files, processes, threads, mutexes, …)
 * by opaque HANDLE values rather than raw pointers.  This module implements
 * a thread-safe handle table that maps HANDLE values to internal object
 * pointers, with:
 *
 *   - Generation counters to detect stale/reused handles
 *   - Reference counting for safe concurrent access
 *   - Type tagging so handle_get_object() can verify the caller's expectation
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "types.h"
#include <pthread.h>
#include <stddef.h>

/* ============================================================================
 * Handle encoding
 *
 * A HANDLE value encodes two pieces of information:
 *   - Bits [0..15]   : slot index into the entries array
 *   - Bits [16..31]  : generation counter (detects use-after-close)
 *
 * This gives a maximum of 65 536 simultaneous handles, which is far beyond
 * what any single PE32 application would need.
 * ============================================================================ */

/** Default initial capacity of the handle table (number of slots). */
#define HANDLE_TABLE_INITIAL_CAPACITY 256

/* ============================================================================
 * Handle types — what kind of kernel object does this handle refer to?
 * ============================================================================ */

typedef enum {
    HANDLE_TYPE_FILE          = 0, /**< File or device handle              */
    HANDLE_TYPE_PROCESS       = 1, /**< Process object                     */
    HANDLE_TYPE_THREAD        = 2, /**< Thread object                      */
    HANDLE_TYPE_MUTEX         = 3, /**< Mutex (mutant) synchronization obj */
    HANDLE_TYPE_EVENT         = 4, /**< Event synchronization object       */
    HANDLE_TYPE_SEMAPHORE     = 5, /**< Semaphore synchronization object   */
    HANDLE_TYPE_REGISTRY_KEY  = 6, /**< Registry key handle                */
} HANDLE_TYPE;

/* ============================================================================
 * Handle table entry
 * ============================================================================ */

/**
 * HANDLE_ENTRY — one slot in the handle table.
 *
 * When in_use is false the slot is free and may be recycled.  The generation
 * counter is incremented each time a slot is recycled, so that stale HANDLE
 * values from a previous occupant are detected.
 */
typedef struct {
    HANDLE_TYPE type;       /**< Object type tag                             */
    void*       object;     /**< Pointer to the underlying host object       */
    uint32_t    ref_count;  /**< Reference count (1 = one owner)             */
    uint32_t    flags;      /**< Reserved for future use (inheritable, etc.) */
    uint16_t    generation; /**< Bumped on every reuse of this slot          */
    bool        in_use;     /**< True if this slot is occupied               */
} HANDLE_ENTRY;

/* ============================================================================
 * Handle table
 * ============================================================================ */

/**
 * HANDLE_TABLE — a dynamically-growable, thread-safe array of handle entries.
 */
typedef struct {
    HANDLE_ENTRY*   entries;   /**< Array of handle entries                  */
    uint32_t        capacity;  /**< Current allocated size of entries[]      */
    uint32_t        count;     /**< Number of slots currently in use         */
    pthread_mutex_t mutex;     /**< Guards all mutations                     */
} HANDLE_TABLE;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * Initialize a handle table with HANDLE_TABLE_INITIAL_CAPACITY slots.
 *
 * Must be called before any other handle operation.  The table must
 * eventually be destroyed with macwi_handle_table_destroy().
 *
 * @param table  The table to initialize.
 * @return MACWI_SUCCESS, or MACWI_ERROR_MEMORY / MACWI_ERROR_INVALID_PARAM.
 */
macwi_status_t macwi_handle_table_init(HANDLE_TABLE* table);

/**
 * Destroy a handle table, releasing all internal memory.
 *
 * Any handles still open at destruction time are silently closed.
 * Using the table after this call is undefined behavior.
 *
 * @param table  The table to destroy.
 */
void macwi_handle_table_destroy(HANDLE_TABLE* table);

/**
 * Create a new handle that refers to the given object.
 *
 * @param table   Handle table (must be initialized).
 * @param type    Type tag for the object.
 * @param object  Pointer to the underlying host-side object (caller retains
 *                ownership; the table only stores the pointer).
 * @return A valid HANDLE on success, or INVALID_HANDLE_VALUE on failure.
 */
HANDLE macwi_handle_create(HANDLE_TABLE* table, HANDLE_TYPE type, void* object);

/**
 * Close a handle, decrementing its reference count.  When the count reaches
 * zero the slot is freed for reuse.
 *
 * @param table   Handle table.
 * @param handle  The handle to close.
 * @return MACWI_SUCCESS, MACWI_ERROR_NOT_FOUND (invalid/stale handle), or
 *         MACWI_ERROR_INVALID_PARAM.
 */
macwi_status_t macwi_handle_close(HANDLE_TABLE* table, HANDLE handle);

/**
 * Retrieve the object pointer associated with a handle, verifying the type.
 *
 * @param table          Handle table.
 * @param handle         The handle to look up.
 * @param expected_type  The type the caller expects; if the actual type differs,
 *                       the call fails with MACWI_ERROR_INVALID_PARAM.
 * @param out_object     Output: the object pointer.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_handle_get_object(HANDLE_TABLE* table, HANDLE handle,
                                        HANDLE_TYPE expected_type,
                                        void** out_object);

/**
 * Duplicate a handle, incrementing its reference count.
 *
 * @param table  Handle table.
 * @param src    The handle to duplicate.
 * @return A new HANDLE value referring to the same object, or
 *         INVALID_HANDLE_VALUE on failure.
 */
HANDLE macwi_handle_duplicate(HANDLE_TABLE* table, HANDLE src);
