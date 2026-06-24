/**
 * @file handle.c
 * @brief Thread-safe handle table implementation.
 *
 * Implements a Win32-style handle table backed by a dynamic array of
 * HANDLE_ENTRY slots.  Each HANDLE value encodes a slot index (low 16 bits)
 * and a generation counter (high 16 bits).  The generation counter is bumped
 * every time a slot is recycled, so that stale handles from a previous
 * occupant are reliably detected.
 *
 * All public functions are guarded by a pthread_mutex for thread safety.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/handle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Handle encoding / decoding
 *
 * HANDLE value layout (cast to uintptr_t):
 *   Bits [0..15]   = slot index  (max 65535)
 *   Bits [16..31]  = generation  (wraps around at 65535)
 * ============================================================================ */

/** Encode a slot index + generation into a HANDLE value. */
static HANDLE internal_encode_handle(uint16_t index, uint16_t generation) {
    uintptr_t val = ((uintptr_t)generation << 16) | (uintptr_t)index;
    return (HANDLE)val;
}

/** Decode the slot index from a HANDLE. */
static uint16_t internal_handle_index(HANDLE handle) {
    return (uint16_t)((uintptr_t)handle & 0xFFFF);
}

/** Decode the generation from a HANDLE. */
static uint16_t internal_handle_generation(HANDLE handle) {
    return (uint16_t)(((uintptr_t)handle >> 16) & 0xFFFF);
}

/**
 * Grow the handle table by doubling its capacity.
 * Caller must hold the mutex.
 */
static macwi_status_t internal_grow_table(HANDLE_TABLE* table) {
    uint32_t new_cap = table->capacity * 2;
    if (new_cap > 0xFFFF) new_cap = 0xFFFF;  /* Hard limit from 16-bit index */

    if (new_cap <= table->capacity) {
        fprintf(stderr, "[macwi:handle] ERROR: Handle table at maximum capacity\n");
        return MACWI_ERROR_MEMORY;
    }

    HANDLE_ENTRY* new_entries = (HANDLE_ENTRY*)realloc(
        table->entries, new_cap * sizeof(HANDLE_ENTRY));

    if (!new_entries) {
        fprintf(stderr, "[macwi:handle] ERROR: realloc failed\n");
        return MACWI_ERROR_MEMORY;
    }

    /* Zero-initialize the newly allocated slots */
    memset(&new_entries[table->capacity], 0,
           (new_cap - table->capacity) * sizeof(HANDLE_ENTRY));

    table->entries  = new_entries;
    table->capacity = new_cap;

    return MACWI_SUCCESS;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

macwi_status_t macwi_handle_table_init(HANDLE_TABLE* table) {
    if (!table) return MACWI_ERROR_INVALID_PARAM;

    memset(table, 0, sizeof(HANDLE_TABLE));

    table->entries = (HANDLE_ENTRY*)calloc(HANDLE_TABLE_INITIAL_CAPACITY,
                                            sizeof(HANDLE_ENTRY));
    if (!table->entries) {
        fprintf(stderr, "[macwi:handle] ERROR: Initial allocation failed\n");
        return MACWI_ERROR_MEMORY;
    }

    table->capacity = HANDLE_TABLE_INITIAL_CAPACITY;
    table->count    = 0;

    if (pthread_mutex_init(&table->mutex, NULL) != 0) {
        free(table->entries);
        table->entries = NULL;
        fprintf(stderr, "[macwi:handle] ERROR: pthread_mutex_init failed\n");
        return MACWI_ERROR_MEMORY;
    }

    return MACWI_SUCCESS;
}

void macwi_handle_table_destroy(HANDLE_TABLE* table) {
    if (!table) return;

    pthread_mutex_lock(&table->mutex);

    /* Warn about leaked handles */
    for (uint32_t i = 0; i < table->capacity; i++) {
        if (table->entries[i].in_use) {
            fprintf(stderr, "[macwi:handle] WARNING: Leaked handle at slot %u "
                    "(type=%d, refs=%u)\n",
                    i, table->entries[i].type, table->entries[i].ref_count);
        }
    }

    free(table->entries);
    table->entries  = NULL;
    table->capacity = 0;
    table->count    = 0;

    pthread_mutex_unlock(&table->mutex);
    pthread_mutex_destroy(&table->mutex);
}

HANDLE macwi_handle_create(HANDLE_TABLE* table, HANDLE_TYPE type, void* object) {
    if (!table) return INVALID_HANDLE_VALUE;

    pthread_mutex_lock(&table->mutex);

    /* Find a free slot */
    uint32_t slot = UINT32_MAX;
    for (uint32_t i = 0; i < table->capacity; i++) {
        if (!table->entries[i].in_use) {
            slot = i;
            break;
        }
    }

    /* No free slot — try to grow */
    if (slot == UINT32_MAX) {
        macwi_status_t status = internal_grow_table(table);
        if (status != MACWI_SUCCESS) {
            pthread_mutex_unlock(&table->mutex);
            return INVALID_HANDLE_VALUE;
        }
        /* The first newly allocated slot */
        slot = table->count;
        /* Search again to find the first free slot after growth */
        for (uint32_t i = 0; i < table->capacity; i++) {
            if (!table->entries[i].in_use) {
                slot = i;
                break;
            }
        }
    }

    /* Populate the slot */
    HANDLE_ENTRY* entry = &table->entries[slot];
    entry->type       = type;
    entry->object     = object;
    entry->ref_count  = 1;
    entry->flags      = 0;
    /* Generation is already set from previous incarnation or 0 from calloc */
    entry->in_use     = true;

    table->count++;

    HANDLE result = internal_encode_handle((uint16_t)slot, entry->generation);

    pthread_mutex_unlock(&table->mutex);

    return result;
}

macwi_status_t macwi_handle_close(HANDLE_TABLE* table, HANDLE handle) {
    if (!table || handle == INVALID_HANDLE_VALUE) {
        return MACWI_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&table->mutex);

    uint16_t index = internal_handle_index(handle);
    uint16_t gen   = internal_handle_generation(handle);

    if (index >= table->capacity) {
        pthread_mutex_unlock(&table->mutex);
        return MACWI_ERROR_NOT_FOUND;
    }

    HANDLE_ENTRY* entry = &table->entries[index];

    if (!entry->in_use || entry->generation != gen) {
        /* Stale or already-closed handle */
        pthread_mutex_unlock(&table->mutex);
        return MACWI_ERROR_NOT_FOUND;
    }

    entry->ref_count--;

    if (entry->ref_count == 0) {
        entry->in_use   = false;
        entry->object   = NULL;
        entry->flags    = 0;
        entry->generation++;  /* Invalidate any remaining copies of this handle */
        table->count--;
    }

    pthread_mutex_unlock(&table->mutex);

    return MACWI_SUCCESS;
}

macwi_status_t macwi_handle_get_object(HANDLE_TABLE* table, HANDLE handle,
                                        HANDLE_TYPE expected_type,
                                        void** out_object) {
    if (!table || !out_object || handle == INVALID_HANDLE_VALUE) {
        return MACWI_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&table->mutex);

    uint16_t index = internal_handle_index(handle);
    uint16_t gen   = internal_handle_generation(handle);

    if (index >= table->capacity) {
        pthread_mutex_unlock(&table->mutex);
        return MACWI_ERROR_NOT_FOUND;
    }

    HANDLE_ENTRY* entry = &table->entries[index];

    if (!entry->in_use || entry->generation != gen) {
        pthread_mutex_unlock(&table->mutex);
        return MACWI_ERROR_NOT_FOUND;
    }

    if (entry->type != expected_type) {
        fprintf(stderr, "[macwi:handle] ERROR: Type mismatch for handle "
                "(expected %d, got %d)\n", expected_type, entry->type);
        pthread_mutex_unlock(&table->mutex);
        return MACWI_ERROR_INVALID_PARAM;
    }

    *out_object = entry->object;

    pthread_mutex_unlock(&table->mutex);

    return MACWI_SUCCESS;
}

HANDLE macwi_handle_duplicate(HANDLE_TABLE* table, HANDLE src) {
    if (!table || src == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }

    pthread_mutex_lock(&table->mutex);

    uint16_t index = internal_handle_index(src);
    uint16_t gen   = internal_handle_generation(src);

    if (index >= table->capacity) {
        pthread_mutex_unlock(&table->mutex);
        return INVALID_HANDLE_VALUE;
    }

    HANDLE_ENTRY* entry = &table->entries[index];

    if (!entry->in_use || entry->generation != gen) {
        pthread_mutex_unlock(&table->mutex);
        return INVALID_HANDLE_VALUE;
    }

    entry->ref_count++;

    /* Return a new handle value with the same index and generation */
    HANDLE result = internal_encode_handle(index, gen);

    pthread_mutex_unlock(&table->mutex);

    return result;
}
