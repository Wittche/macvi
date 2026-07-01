/**
 * @file advapi32.c
 * @brief Win32 advapi32.dll stub implementations using the EMU_CONTEXT dispatcher.
 *
 * SPDX-License-Identifier: MIT
 */

#include "advapi32.h"
#include "macwi/types.h"
#include "macwi/emu.h"
#include "macwi/thunk.h"
#include "macwi/registry.h"
#include "macwi/handle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADVAPI_STUB_LOG(fmt, ...) fprintf(stderr, "[macwi:advapi32] " fmt "\n", ##__VA_ARGS__)

extern HANDLE_TABLE g_macwi_handle_table;

/* ============================================================================
 * RegOpenKeyExA
 * ============================================================================ */
static void win32_RegOpenKeyExA(EMU_CONTEXT* ctx) {
    uint64_t hKey, lpSubKey, ulOptions, samDesired, phkResult;
    macwi_thunk_read_param_64(ctx, 0, &hKey);
    macwi_thunk_read_param_64(ctx, 1, &lpSubKey);
    macwi_thunk_read_param_64(ctx, 2, &ulOptions);
    macwi_thunk_read_param_64(ctx, 3, &samDesired);
    macwi_thunk_read_param_64(ctx, 4, &phkResult);

    char subkey[256] = {0};
    if (lpSubKey) macwi_thunk_read_guest_string(ctx, lpSubKey, subkey, sizeof(subkey));

    ADVAPI_STUB_LOG("RegOpenKeyExA(hKey=0x%llX, subkey=\"%s\")", hKey, subkey);

    void* key_obj = NULL;
    if (macwi_reg_open_key((uint64_t)hKey, subkey, &key_obj) == MACWI_SUCCESS) {
        HANDLE h = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_REGISTRY_KEY, key_obj);
        uint64_t h32 = (uint64_t)(uintptr_t)h;
        macwi_emu_write_memory(ctx, phkResult, &h32, 4);
        macwi_emu_reg_write_64(ctx, 0, 0); // ERROR_SUCCESS
    } else {
        macwi_emu_reg_write_64(ctx, 0, 2); // ERROR_FILE_NOT_FOUND
    }
    macwi_thunk_stdcall_return(ctx, 5);
}

/* ============================================================================
 * RegCreateKeyExA
 * ============================================================================ */
static void win32_RegCreateKeyExA(EMU_CONTEXT* ctx) {
    uint64_t hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition;
    macwi_thunk_read_param_64(ctx, 0, &hKey);
    macwi_thunk_read_param_64(ctx, 1, &lpSubKey);
    macwi_thunk_read_param_64(ctx, 2, &Reserved);
    macwi_thunk_read_param_64(ctx, 3, &lpClass);
    macwi_thunk_read_param_64(ctx, 4, &dwOptions);
    macwi_thunk_read_param_64(ctx, 5, &samDesired);
    macwi_thunk_read_param_64(ctx, 6, &lpSecurityAttributes);
    macwi_thunk_read_param_64(ctx, 7, &phkResult);
    macwi_thunk_read_param_64(ctx, 8, &lpdwDisposition);

    char subkey[256] = {0};
    if (lpSubKey) macwi_thunk_read_guest_string(ctx, lpSubKey, subkey, sizeof(subkey));

    ADVAPI_STUB_LOG("RegCreateKeyExA(hKey=0x%llX, subkey=\"%s\")", hKey, subkey);

    void* key_obj = NULL;
    if (macwi_reg_create_key((uint64_t)hKey, subkey, &key_obj) == MACWI_SUCCESS) {
        HANDLE h = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_REGISTRY_KEY, key_obj);
        uint64_t h32 = (uint64_t)(uintptr_t)h;
        macwi_emu_write_memory(ctx, phkResult, &h32, 4);
        if (lpdwDisposition) {
            uint64_t disp = 1; // REG_CREATED_NEW_KEY
            macwi_emu_write_memory(ctx, lpdwDisposition, &disp, 4);
        }
        macwi_emu_reg_write_64(ctx, 0, 0); // ERROR_SUCCESS
    } else {
        macwi_emu_reg_write_64(ctx, 0, 2); // ERROR_FILE_NOT_FOUND
    }
    macwi_thunk_stdcall_return(ctx, 9);
}

/* ============================================================================
 * RegSetValueExA
 * ============================================================================ */
static void win32_RegSetValueExA(EMU_CONTEXT* ctx) {
    uint64_t hKey, lpValueName, Reserved, dwType, lpData, cbData;
    macwi_thunk_read_param_64(ctx, 0, &hKey);
    macwi_thunk_read_param_64(ctx, 1, &lpValueName);
    macwi_thunk_read_param_64(ctx, 2, &Reserved);
    macwi_thunk_read_param_64(ctx, 3, &dwType);
    macwi_thunk_read_param_64(ctx, 4, &lpData);
    macwi_thunk_read_param_64(ctx, 5, &cbData);

    char valname[256] = {0};
    if (lpValueName) macwi_thunk_read_guest_string(ctx, lpValueName, valname, sizeof(valname));

    ADVAPI_STUB_LOG("RegSetValueExA(hKey=0x%llX, valname=\"%s\", type=%u, cbData=%u)", hKey, valname, (uint64_t)dwType, (uint64_t)cbData);

    void* key_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hKey, HANDLE_TYPE_REGISTRY_KEY, &key_obj) != MACWI_SUCCESS) {
        macwi_emu_reg_write_64(ctx, 0, 6); // ERROR_INVALID_HANDLE
        macwi_thunk_stdcall_return(ctx, 6);
        return;
    }

    uint8_t* temp = (uint8_t*)malloc(cbData > 0 ? cbData : 1);
    if (cbData > 0) macwi_emu_read_memory(ctx, lpData, temp, cbData);

    macwi_status_t st = macwi_reg_set_value(key_obj, valname, (uint64_t)dwType, temp, (uint64_t)cbData);
    free(temp);

    macwi_emu_reg_write_64(ctx, 0, (st == MACWI_SUCCESS) ? 0 : 87);
    macwi_thunk_stdcall_return(ctx, 6);
}

/* ============================================================================
 * RegQueryValueExA
 * ============================================================================ */
static void win32_RegQueryValueExA(EMU_CONTEXT* ctx) {
    uint64_t hKey, lpValueName, lpReserved, lpType, lpData, lpcbData;
    macwi_thunk_read_param_64(ctx, 0, &hKey);
    macwi_thunk_read_param_64(ctx, 1, &lpValueName);
    macwi_thunk_read_param_64(ctx, 2, &lpReserved);
    macwi_thunk_read_param_64(ctx, 3, &lpType);
    macwi_thunk_read_param_64(ctx, 4, &lpData);
    macwi_thunk_read_param_64(ctx, 5, &lpcbData);

    char valname[256] = {0};
    if (lpValueName) macwi_thunk_read_guest_string(ctx, lpValueName, valname, sizeof(valname));

    ADVAPI_STUB_LOG("RegQueryValueExA(hKey=0x%llX, valname=\"%s\")", hKey, valname);

    void* key_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hKey, HANDLE_TYPE_REGISTRY_KEY, &key_obj) != MACWI_SUCCESS) {
        macwi_emu_reg_write_64(ctx, 0, 6); // ERROR_INVALID_HANDLE
        macwi_thunk_stdcall_return(ctx, 6);
        return;
    }

    uint64_t type = 0;
    uint64_t size = 0;

    if (lpcbData) macwi_emu_read_memory(ctx, lpcbData, &size, 4);

    uint8_t* temp = NULL;
    if (lpData && size > 0) {
        temp = (uint8_t*)malloc(size);
    }

    macwi_status_t st = macwi_reg_query_value(key_obj, valname, &type, temp, &size);

    if (st == MACWI_SUCCESS) {
        if (lpType) macwi_emu_write_memory(ctx, lpType, &type, 4);
        if (lpcbData) macwi_emu_write_memory(ctx, lpcbData, &size, 4);
        if (lpData && temp) {
            macwi_emu_write_memory(ctx, lpData, temp, size);
        }
        macwi_emu_reg_write_64(ctx, 0, 0); // ERROR_SUCCESS
    } else {
        macwi_emu_reg_write_64(ctx, 0, 2); // ERROR_FILE_NOT_FOUND
    }

    if (temp) free(temp);
    macwi_thunk_stdcall_return(ctx, 6);
}

/* ============================================================================
 * RegDeleteKeyA
 * ============================================================================ */
static void win32_RegDeleteKeyA(EMU_CONTEXT* ctx) {
    uint64_t hKey, lpSubKey;
    macwi_thunk_read_param_64(ctx, 0, &hKey);
    macwi_thunk_read_param_64(ctx, 1, &lpSubKey);

    char subkey[256] = {0};
    if (lpSubKey) macwi_thunk_read_guest_string(ctx, lpSubKey, subkey, sizeof(subkey));

    ADVAPI_STUB_LOG("RegDeleteKeyA(hKey=0x%llX, subkey=\"%s\")", hKey, subkey);

    macwi_status_t st = macwi_reg_delete_key((uint64_t)hKey, subkey);
    macwi_emu_reg_write_64(ctx, 0, (st == MACWI_SUCCESS) ? 0 : 2);
    macwi_thunk_stdcall_return(ctx, 2);
}

/* ============================================================================
 * RegDeleteValueA
 * ============================================================================ */
static void win32_RegDeleteValueA(EMU_CONTEXT* ctx) {
    uint64_t hKey, lpValueName;
    macwi_thunk_read_param_64(ctx, 0, &hKey);
    macwi_thunk_read_param_64(ctx, 1, &lpValueName);

    char valname[256] = {0};
    if (lpValueName) macwi_thunk_read_guest_string(ctx, lpValueName, valname, sizeof(valname));

    ADVAPI_STUB_LOG("RegDeleteValueA(hKey=0x%llX, valname=\"%s\")", hKey, valname);

    void* key_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hKey, HANDLE_TYPE_REGISTRY_KEY, &key_obj) != MACWI_SUCCESS) {
        macwi_emu_reg_write_64(ctx, 0, 6); // ERROR_INVALID_HANDLE
        macwi_thunk_stdcall_return(ctx, 2);
        return;
    }

    macwi_status_t st = macwi_reg_delete_value(key_obj, valname);
    macwi_emu_reg_write_64(ctx, 0, (st == MACWI_SUCCESS) ? 0 : 2);
    macwi_thunk_stdcall_return(ctx, 2);
}

/* ============================================================================
 * RegEnumKeyExA
 * ============================================================================ */
static void win32_RegEnumKeyExA(EMU_CONTEXT* ctx) {
    uint64_t hKey, dwIndex, lpName, lpcchName, lpReserved, lpClass, lpcchClass, lpftLastWriteTime;
    macwi_thunk_read_param_64(ctx, 0, &hKey);
    macwi_thunk_read_param_64(ctx, 1, &dwIndex);
    macwi_thunk_read_param_64(ctx, 2, &lpName);
    macwi_thunk_read_param_64(ctx, 3, &lpcchName);
    macwi_thunk_read_param_64(ctx, 4, &lpReserved);
    macwi_thunk_read_param_64(ctx, 5, &lpClass);
    macwi_thunk_read_param_64(ctx, 6, &lpcchClass);
    macwi_thunk_read_param_64(ctx, 7, &lpftLastWriteTime);

    ADVAPI_STUB_LOG("RegEnumKeyExA(hKey=0x%llX, index=%u)", hKey, (uint64_t)dwIndex);

    void* key_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hKey, HANDLE_TYPE_REGISTRY_KEY, &key_obj) != MACWI_SUCCESS) {
        macwi_emu_reg_write_64(ctx, 0, 6); // ERROR_INVALID_HANDLE
        macwi_thunk_stdcall_return(ctx, 8);
        return;
    }

    char name_buf[256] = {0};
    uint64_t name_len = sizeof(name_buf);

    macwi_status_t st = macwi_reg_enum_key(key_obj, (uint64_t)dwIndex, name_buf, &name_len);

    if (st == MACWI_SUCCESS) {
        if (lpName) macwi_emu_write_memory(ctx, lpName, name_buf, name_len + 1);
        if (lpcchName) macwi_emu_write_memory(ctx, lpcchName, &name_len, 4);
        macwi_emu_reg_write_64(ctx, 0, 0); // ERROR_SUCCESS
    } else {
        macwi_emu_reg_write_64(ctx, 0, 259); // ERROR_NO_MORE_ITEMS
    }
    macwi_thunk_stdcall_return(ctx, 8);
}

/* ============================================================================
 * RegEnumValueA
 * ============================================================================ */
static void win32_RegEnumValueA(EMU_CONTEXT* ctx) {
    uint64_t hKey, dwIndex, lpValueName, lpcchValueName, lpReserved, lpType, lpData, lpcbData;
    macwi_thunk_read_param_64(ctx, 0, &hKey);
    macwi_thunk_read_param_64(ctx, 1, &dwIndex);
    macwi_thunk_read_param_64(ctx, 2, &lpValueName);
    macwi_thunk_read_param_64(ctx, 3, &lpcchValueName);
    macwi_thunk_read_param_64(ctx, 4, &lpReserved);
    macwi_thunk_read_param_64(ctx, 5, &lpType);
    macwi_thunk_read_param_64(ctx, 6, &lpData);
    macwi_thunk_read_param_64(ctx, 7, &lpcbData);

    ADVAPI_STUB_LOG("RegEnumValueA(hKey=0x%llX, index=%u)", hKey, (uint64_t)dwIndex);

    void* key_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hKey, HANDLE_TYPE_REGISTRY_KEY, &key_obj) != MACWI_SUCCESS) {
        macwi_emu_reg_write_64(ctx, 0, 6);
        macwi_thunk_stdcall_return(ctx, 8);
        return;
    }

    char vname_buf[256] = {0};
    uint64_t vname_len = sizeof(vname_buf);
    uint64_t type = 0;
    uint8_t data_buf[1024] = {0};
    uint64_t data_len = sizeof(data_buf);

    macwi_status_t st = macwi_reg_enum_value(key_obj, (uint64_t)dwIndex, vname_buf, &vname_len, &type, data_buf, &data_len);

    if (st == MACWI_SUCCESS) {
        if (lpValueName) macwi_emu_write_memory(ctx, lpValueName, vname_buf, vname_len + 1);
        if (lpcchValueName) macwi_emu_write_memory(ctx, lpcchValueName, &vname_len, 4);
        if (lpType) macwi_emu_write_memory(ctx, lpType, &type, 4);
        if (lpData && lpcbData) macwi_emu_write_memory(ctx, lpData, data_buf, data_len);
        if (lpcbData) macwi_emu_write_memory(ctx, lpcbData, &data_len, 4);
        macwi_emu_reg_write_64(ctx, 0, 0);
    } else {
        macwi_emu_reg_write_64(ctx, 0, 259); // ERROR_NO_MORE_ITEMS
    }
    macwi_thunk_stdcall_return(ctx, 8);
}

/* ============================================================================
 * RegCloseKey
 * ============================================================================ */
static void win32_RegCloseKey(EMU_CONTEXT* ctx) {
    uint64_t hKey;
    macwi_thunk_read_param_64(ctx, 0, &hKey);
    ADVAPI_STUB_LOG("RegCloseKey(0x%llX)", hKey);

    macwi_status_t st = macwi_handle_close(&g_macwi_handle_table, (HANDLE)(uintptr_t)hKey);
    macwi_emu_reg_write_64(ctx, 0, (st == MACWI_SUCCESS) ? 0 : 6);

    // Auto-save registry on close
    macwi_registry_save();

    macwi_thunk_stdcall_return(ctx, 1);
}

/* ============================================================================
 * API Registration
 * ============================================================================ */

void macwi_advapi32_register_apis(void) {
    macwi_thunk_register_api("advapi32.dll", "RegOpenKeyExA",    win32_RegOpenKeyExA, 5);
    macwi_thunk_register_api("advapi32.dll", "RegCreateKeyExA",  win32_RegCreateKeyExA, 9);
    macwi_thunk_register_api("advapi32.dll", "RegSetValueExA",   win32_RegSetValueExA, 6);
    macwi_thunk_register_api("advapi32.dll", "RegQueryValueExA", win32_RegQueryValueExA, 6);
    macwi_thunk_register_api("advapi32.dll", "RegDeleteKeyA",    win32_RegDeleteKeyA, 2);
    macwi_thunk_register_api("advapi32.dll", "RegDeleteValueA",  win32_RegDeleteValueA, 2);
    macwi_thunk_register_api("advapi32.dll", "RegEnumKeyExA",    win32_RegEnumKeyExA, 8);
    macwi_thunk_register_api("advapi32.dll", "RegEnumValueA",    win32_RegEnumValueA, 8);
    macwi_thunk_register_api("advapi32.dll", "RegCloseKey",      win32_RegCloseKey, 1);
}
