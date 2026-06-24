/**
 * @file registry.c
 * @brief Simple in-memory registry implementation.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

typedef struct RegValue {
    char* name;
    uint32_t type;
    uint8_t* data;
    uint32_t data_len;
    struct RegValue* next;
} RegValue;

typedef struct RegKey {
    char* name;
    struct RegKey* subkeys;
    struct RegKey* next;
    RegValue* values;
} RegKey;

static RegKey* g_root_hklm = NULL;
static RegKey* g_root_hkcu = NULL;
static pthread_mutex_t g_reg_mutex = PTHREAD_MUTEX_INITIALIZER;

static RegKey* create_key_node(const char* name) {
    RegKey* k = (RegKey*)calloc(1, sizeof(RegKey));
    if (k) {
        k->name = strdup(name ? name : "");
    }
    return k;
}

macwi_status_t macwi_registry_init(void) {
    pthread_mutex_lock(&g_reg_mutex);
    if (!g_root_hklm) g_root_hklm = create_key_node("HKEY_LOCAL_MACHINE");
    if (!g_root_hkcu) g_root_hkcu = create_key_node("HKEY_CURRENT_USER");
    pthread_mutex_unlock(&g_reg_mutex);
    return MACWI_SUCCESS;
}

static RegKey* get_root_key(uint32_t hKey) {
    if (hKey == HKEY_LOCAL_MACHINE) return g_root_hklm;
    if (hKey == HKEY_CURRENT_USER) return g_root_hkcu;
    return NULL;
}

static RegKey* find_subkey(RegKey* parent, const char* name) {
    for (RegKey* k = parent->subkeys; k != NULL; k = k->next) {
        if (strcasecmp(k->name, name) == 0) return k;
    }
    return NULL;
}

static RegKey* navigate_path(RegKey* root, const char* path, int create) {
    if (!root) return NULL;
    if (!path || path[0] == '\0') return root;

    char* path_copy = strdup(path);
    char* token = strtok(path_copy, "\\/");
    RegKey* current = root;

    while (token != NULL) {
        RegKey* next = find_subkey(current, token);
        if (!next) {
            if (!create) {
                current = NULL;
                break;
            }
            next = create_key_node(token);
            next->next = current->subkeys;
            current->subkeys = next;
        }
        current = next;
        token = strtok(NULL, "\\/");
    }

    free(path_copy);
    return current;
}

macwi_status_t macwi_reg_open_key(uint32_t hKey, const char* lpSubKey, void** out_key_obj) {
    pthread_mutex_lock(&g_reg_mutex);
    
    // If hKey is already an object pointer (not a root handle macro), use it.
    // We differentiate by checking if hKey has the high bit set.
    RegKey* base = (hKey & 0x80000000) ? get_root_key(hKey) : (RegKey*)(uintptr_t)hKey;
    if (!base) {
        pthread_mutex_unlock(&g_reg_mutex);
        return MACWI_ERROR_NOT_FOUND;
    }

    RegKey* target = navigate_path(base, lpSubKey, 0);
    if (!target) {
        pthread_mutex_unlock(&g_reg_mutex);
        return MACWI_ERROR_NOT_FOUND;
    }

    *out_key_obj = target;
    pthread_mutex_unlock(&g_reg_mutex);
    return MACWI_SUCCESS;
}

macwi_status_t macwi_reg_create_key(uint32_t hKey, const char* lpSubKey, void** out_key_obj) {
    pthread_mutex_lock(&g_reg_mutex);
    
    RegKey* base = (hKey & 0x80000000) ? get_root_key(hKey) : (RegKey*)(uintptr_t)hKey;
    if (!base) {
        pthread_mutex_unlock(&g_reg_mutex);
        return MACWI_ERROR_NOT_FOUND;
    }

    RegKey* target = navigate_path(base, lpSubKey, 1);
    *out_key_obj = target;
    pthread_mutex_unlock(&g_reg_mutex);
    return MACWI_SUCCESS;
}

macwi_status_t macwi_reg_set_value(void* key_obj, const char* lpValueName, uint32_t dwType, const uint8_t* lpData, uint32_t cbData) {
    if (!key_obj) return MACWI_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_reg_mutex);
    
    RegKey* k = (RegKey*)key_obj;
    const char* vname = lpValueName ? lpValueName : "";

    RegValue* v = k->values;
    while (v) {
        if (strcasecmp(v->name, vname) == 0) break;
        v = v->next;
    }

    if (!v) {
        v = (RegValue*)calloc(1, sizeof(RegValue));
        v->name = strdup(vname);
        v->next = k->values;
        k->values = v;
    } else {
        free(v->data);
    }

    v->type = dwType;
    v->data_len = cbData;
    v->data = (uint8_t*)malloc(cbData);
    if (cbData > 0 && lpData) {
        memcpy(v->data, lpData, cbData);
    }

    pthread_mutex_unlock(&g_reg_mutex);
    return MACWI_SUCCESS;
}

macwi_status_t macwi_reg_query_value(void* key_obj, const char* lpValueName, uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData) {
    if (!key_obj) return MACWI_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_reg_mutex);
    
    RegKey* k = (RegKey*)key_obj;
    const char* vname = lpValueName ? lpValueName : "";

    RegValue* v = k->values;
    while (v) {
        if (strcasecmp(v->name, vname) == 0) break;
        v = v->next;
    }

    if (!v) {
        pthread_mutex_unlock(&g_reg_mutex);
        return MACWI_ERROR_NOT_FOUND;
    }

    if (lpType) *lpType = v->type;

    if (lpData && lpcbData && *lpcbData >= v->data_len) {
        memcpy(lpData, v->data, v->data_len);
    }

    if (lpcbData) {
        *lpcbData = v->data_len;
    }

    pthread_mutex_unlock(&g_reg_mutex);
    return MACWI_SUCCESS;
}
