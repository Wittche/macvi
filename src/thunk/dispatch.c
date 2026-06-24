/**
 * @file dispatch.c
 * @brief Thunking layer — API Dispatcher using Unicorn hooks.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/thunk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC_VA_BASE 0x70000000
#define MAGIC_VA_SIZE 0x1000000   // 16MB for API stubs
#define MAX_APIS      4096

typedef struct {
    char dll_name[64];
    char func_name[64];
    macwi_win32_api_cb callback;
    int param_count;
    uint32_t magic_va;
} API_ENTRY;

static API_ENTRY g_apis[MAX_APIS];
static int g_api_count = 0;

macwi_status_t macwi_thunk_register_api(const char* dll_name, const char* func_name, macwi_win32_api_cb callback, int param_count) {
    if (!dll_name || !func_name || !callback) return MACWI_ERROR_INVALID_PARAM;
    if (g_api_count >= MAX_APIS) return MACWI_ERROR_MEMORY;

    API_ENTRY* entry = &g_apis[g_api_count];
    strncpy(entry->dll_name, dll_name, sizeof(entry->dll_name) - 1);
    strncpy(entry->func_name, func_name, sizeof(entry->func_name) - 1);
    entry->callback = callback;
    entry->param_count = param_count;
    
    // Assign a unique Magic VA (space them by 4 bytes)
    entry->magic_va = MAGIC_VA_BASE + (g_api_count * 4);
    
    g_api_count++;
    return MACWI_SUCCESS;
}

uint32_t macwi_thunk_get_magic_va(const char* dll_name, const char* func_name) {
    for (int i = 0; i < g_api_count; i++) {
        if (strcasecmp(g_apis[i].dll_name, dll_name) == 0 &&
            strcmp(g_apis[i].func_name, func_name) == 0) {
            return g_apis[i].magic_va;
        }
    }
    return 0; // Not found
}

static void api_hook_callback(EMU_CONTEXT* ctx, uint32_t address, uint32_t size, void* user_data) {
    (void)size;
    (void)user_data;

    if (address >= MAGIC_VA_BASE && address < MAGIC_VA_BASE + (g_api_count * 4)) {
        int api_idx = (address - MAGIC_VA_BASE) / 4;
        if (api_idx >= 0 && api_idx < g_api_count) {
            API_ENTRY* api = &g_apis[api_idx];
            
            // Execute native callback
            api->callback(ctx);

            // Simulate stdcall return:
            // 1. Read return address from stack (ESP)
            uint32_t esp = 0;
            macwi_emu_reg_read(ctx, 7 /* ESP */, &esp);
            uint32_t ret_addr = 0;
            macwi_emu_read_memory(ctx, esp, &ret_addr, 4);

            // 2. Adjust ESP (pop return address + params)
            esp += 4 + (api->param_count * 4);
            macwi_emu_reg_write(ctx, 7 /* ESP */, esp);

            // 3. Set EIP to return address
            macwi_emu_reg_write(ctx, 8 /* EIP */, ret_addr);
        }
    }
}

macwi_status_t macwi_thunk_init_dispatcher(EMU_CONTEXT* ctx) {
    if (!ctx) return MACWI_ERROR_INVALID_PARAM;

    // Allocate the magic memory region in the emulator
    macwi_status_t status = macwi_emu_map_memory(ctx, MAGIC_VA_BASE, MAGIC_VA_SIZE, MACWI_PROT_ALL);
    if (status != MACWI_SUCCESS) {
        fprintf(stderr, "[macwi_thunk] Failed to map magic API region.\n");
        return status;
    }

    // Fill the memory with INT3 (0xCC) or RET (0xC3) just in case
    uint8_t* dummy = (uint8_t*)malloc(MAGIC_VA_SIZE);
    memset(dummy, 0xC3, MAGIC_VA_SIZE);
    macwi_emu_write_memory(ctx, MAGIC_VA_BASE, dummy, MAGIC_VA_SIZE);
    free(dummy);

    // Register the Unicorn code hook for this region
    return macwi_emu_add_code_hook(ctx, MAGIC_VA_BASE, MAGIC_VA_BASE + MAGIC_VA_SIZE - 1, api_hook_callback, NULL, NULL);
}
