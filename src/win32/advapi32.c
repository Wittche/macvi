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

static void win32_RegOpenKeyExA(EMU_CONTEXT* ctx) {
    uint32_t hKey, lpSubKey, ulOptions, samDesired, phkResult;
    macwi_thunk_read_param_32(ctx, 0, &hKey);
    macwi_thunk_read_param_32(ctx, 1, &lpSubKey);
    macwi_thunk_read_param_32(ctx, 2, &ulOptions);
    macwi_thunk_read_param_32(ctx, 3, &samDesired);
    macwi_thunk_read_param_32(ctx, 4, &phkResult);

    char subkey[256] = {0};
    if (lpSubKey) macwi_thunk_read_guest_string(ctx, lpSubKey, subkey, sizeof(subkey));

    ADVAPI_STUB_LOG("RegOpenKeyExA(hKey=0x%X, subkey=\"%s\")", hKey, subkey);

    void* key_obj = NULL;
    if (macwi_reg_open_key(hKey, subkey, &key_obj) == MACWI_SUCCESS) {
        HANDLE h = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_REGISTRY_KEY, key_obj);
        uint32_t h32 = (uint32_t)(uintptr_t)h;
        macwi_emu_write_memory(ctx, phkResult, &h32, 4);
        macwi_emu_reg_write(ctx, 0, 0); // ERROR_SUCCESS
    } else {
        macwi_emu_reg_write(ctx, 0, 2); // ERROR_FILE_NOT_FOUND
    }
}

static void win32_RegCreateKeyExA(EMU_CONTEXT* ctx) {
    uint32_t hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition;
    macwi_thunk_read_param_32(ctx, 0, &hKey);
    macwi_thunk_read_param_32(ctx, 1, &lpSubKey);
    macwi_thunk_read_param_32(ctx, 2, &Reserved);
    macwi_thunk_read_param_32(ctx, 3, &lpClass);
    macwi_thunk_read_param_32(ctx, 4, &dwOptions);
    macwi_thunk_read_param_32(ctx, 5, &samDesired);
    macwi_thunk_read_param_32(ctx, 6, &lpSecurityAttributes);
    macwi_thunk_read_param_32(ctx, 7, &phkResult);
    macwi_thunk_read_param_32(ctx, 8, &lpdwDisposition);

    char subkey[256] = {0};
    if (lpSubKey) macwi_thunk_read_guest_string(ctx, lpSubKey, subkey, sizeof(subkey));

    ADVAPI_STUB_LOG("RegCreateKeyExA(hKey=0x%X, subkey=\"%s\")", hKey, subkey);

    void* key_obj = NULL;
    if (macwi_reg_create_key(hKey, subkey, &key_obj) == MACWI_SUCCESS) {
        HANDLE h = macwi_handle_create(&g_macwi_handle_table, HANDLE_TYPE_REGISTRY_KEY, key_obj);
        uint32_t h32 = (uint32_t)(uintptr_t)h;
        macwi_emu_write_memory(ctx, phkResult, &h32, 4);
        if (lpdwDisposition) {
            uint32_t disp = 1; // REG_CREATED_NEW_KEY
            macwi_emu_write_memory(ctx, lpdwDisposition, &disp, 4);
        }
        macwi_emu_reg_write(ctx, 0, 0); // ERROR_SUCCESS
    } else {
        macwi_emu_reg_write(ctx, 0, 2); // ERROR_FILE_NOT_FOUND
    }
}

static void win32_RegSetValueExA(EMU_CONTEXT* ctx) {
    uint32_t hKey, lpValueName, Reserved, dwType, lpData, cbData;
    macwi_thunk_read_param_32(ctx, 0, &hKey);
    macwi_thunk_read_param_32(ctx, 1, &lpValueName);
    macwi_thunk_read_param_32(ctx, 2, &Reserved);
    macwi_thunk_read_param_32(ctx, 3, &dwType);
    macwi_thunk_read_param_32(ctx, 4, &lpData);
    macwi_thunk_read_param_32(ctx, 5, &cbData);

    char valname[256] = {0};
    if (lpValueName) macwi_thunk_read_guest_string(ctx, lpValueName, valname, sizeof(valname));

    ADVAPI_STUB_LOG("RegSetValueExA(hKey=0x%X, valname=\"%s\")", hKey, valname);

    void* key_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hKey, HANDLE_TYPE_REGISTRY_KEY, &key_obj) != MACWI_SUCCESS) {
        macwi_emu_reg_write(ctx, 0, 6); // ERROR_INVALID_HANDLE
        return;
    }

    uint8_t* temp = (uint8_t*)malloc(cbData);
    if (cbData > 0) macwi_emu_read_memory(ctx, lpData, temp, cbData);

    macwi_status_t st = macwi_reg_set_value(key_obj, valname, dwType, temp, cbData);
    free(temp);

    macwi_emu_reg_write(ctx, 0, (st == MACWI_SUCCESS) ? 0 : 87);
}

static void win32_RegQueryValueExA(EMU_CONTEXT* ctx) {
    uint32_t hKey, lpValueName, lpReserved, lpType, lpData, lpcbData;
    macwi_thunk_read_param_32(ctx, 0, &hKey);
    macwi_thunk_read_param_32(ctx, 1, &lpValueName);
    macwi_thunk_read_param_32(ctx, 2, &lpReserved);
    macwi_thunk_read_param_32(ctx, 3, &lpType);
    macwi_thunk_read_param_32(ctx, 4, &lpData);
    macwi_thunk_read_param_32(ctx, 5, &lpcbData);

    char valname[256] = {0};
    if (lpValueName) macwi_thunk_read_guest_string(ctx, lpValueName, valname, sizeof(valname));

    ADVAPI_STUB_LOG("RegQueryValueExA(hKey=0x%X, valname=\"%s\")", hKey, valname);

    void* key_obj = NULL;
    if (macwi_handle_get_object(&g_macwi_handle_table, (HANDLE)(uintptr_t)hKey, HANDLE_TYPE_REGISTRY_KEY, &key_obj) != MACWI_SUCCESS) {
        macwi_emu_reg_write(ctx, 0, 6); // ERROR_INVALID_HANDLE
        return;
    }

    uint32_t cb = 0;
    if (lpcbData) macwi_emu_read_memory(ctx, lpcbData, &cb, 4);

    uint32_t type = 0;
    uint8_t* temp = NULL;
    if (cb > 0) temp = (uint8_t*)malloc(cb);

    macwi_status_t st = macwi_reg_query_value(key_obj, valname, &type, temp, &cb);

    if (st == MACWI_SUCCESS) {
        if (lpType) macwi_emu_write_memory(ctx, lpType, &type, 4);
        if (lpData && temp) macwi_emu_write_memory(ctx, lpData, temp, cb);
        if (lpcbData) macwi_emu_write_memory(ctx, lpcbData, &cb, 4);
        macwi_emu_reg_write(ctx, 0, 0); // ERROR_SUCCESS
    } else {
        macwi_emu_reg_write(ctx, 0, 2); // ERROR_FILE_NOT_FOUND
    }
    if (temp) free(temp);
}

static void win32_RegCloseKey(EMU_CONTEXT* ctx) {
    uint32_t hKey;
    macwi_thunk_read_param_32(ctx, 0, &hKey);
    ADVAPI_STUB_LOG("RegCloseKey(0x%X)", hKey);
    macwi_handle_close(&g_macwi_handle_table, (HANDLE)(uintptr_t)hKey);
    macwi_emu_reg_write(ctx, 0, 0); // ERROR_SUCCESS
}

void macwi_advapi32_register_apis(void) {
    macwi_thunk_register_api("advapi32.dll", "RegOpenKeyExA", win32_RegOpenKeyExA, 5);
    macwi_thunk_register_api("advapi32.dll", "RegCreateKeyExA", win32_RegCreateKeyExA, 9);
    macwi_thunk_register_api("advapi32.dll", "RegSetValueExA", win32_RegSetValueExA, 6);
    macwi_thunk_register_api("advapi32.dll", "RegQueryValueExA", win32_RegQueryValueExA, 6);
    macwi_thunk_register_api("advapi32.dll", "RegCloseKey", win32_RegCloseKey, 1);
}
