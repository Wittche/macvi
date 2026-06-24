/**
 * @file registry.h
 * @brief Registry emulation — In-memory tree for Windows Registry.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "macwi/types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HKEY_CLASSES_ROOT     0x80000000
#define HKEY_CURRENT_USER     0x80000001
#define HKEY_LOCAL_MACHINE    0x80000002
#define HKEY_USERS            0x80000003

#define REG_NONE              0
#define REG_SZ                1
#define REG_EXPAND_SZ         2
#define REG_BINARY            3
#define REG_DWORD             4

// Base initialization for the registry subsystem
macwi_status_t macwi_registry_init(void);

// Registry API
macwi_status_t macwi_reg_open_key(uint32_t hKey, const char* lpSubKey, void** out_key_obj);
macwi_status_t macwi_reg_create_key(uint32_t hKey, const char* lpSubKey, void** out_key_obj);
macwi_status_t macwi_reg_set_value(void* key_obj, const char* lpValueName, uint32_t dwType, const uint8_t* lpData, uint32_t cbData);
macwi_status_t macwi_reg_query_value(void* key_obj, const char* lpValueName, uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData);

#ifdef __cplusplus
}
#endif
