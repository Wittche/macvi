#pragma once

#include "macwi/types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Registry value types (Win32 constants)
#define MACWI_REG_NONE                    0
#define MACWI_REG_SZ                      1
#define MACWI_REG_EXPAND_SZ               2
#define MACWI_REG_BINARY                  3
#define MACWI_REG_DWORD                   4
#define MACWI_REG_DWORD_LITTLE_ENDIAN     4
#define MACWI_REG_DWORD_BIG_ENDIAN        5
#define MACWI_REG_LINK                    6
#define MACWI_REG_MULTI_SZ                7

// Predefined root keys
#define MACWI_HKEY_CLASSES_ROOT     0x80000000
#define MACWI_HKEY_CURRENT_USER     0x80000001
#define MACWI_HKEY_LOCAL_MACHINE    0x80000002
#define MACWI_HKEY_USERS            0x80000003

typedef struct MACWI_REG_VALUE {
    char* name;
    uint32_t type;
    uint8_t* data;
    uint32_t data_size;
    struct MACWI_REG_VALUE* next;
} MACWI_REG_VALUE;

typedef struct MACWI_REG_KEY {
    char* name;
    char* full_path;
    MACWI_REG_VALUE* values;
    struct MACWI_REG_KEY* parent;
    struct MACWI_REG_KEY* first_child;
    struct MACWI_REG_KEY* next_sibling;
} MACWI_REG_KEY;

// Registry core API
macwi_status_t macwi_reg_init(void);
void macwi_reg_shutdown(void);

// Key operations
macwi_status_t macwi_reg_create_key(MACWI_REG_KEY* hKey, const char* lpSubKey, MACWI_REG_KEY** phkResult);
macwi_status_t macwi_reg_open_key(MACWI_REG_KEY* hKey, const char* lpSubKey, MACWI_REG_KEY** phkResult);
macwi_status_t macwi_reg_close_key(MACWI_REG_KEY* hKey);

// Value operations
macwi_status_t macwi_reg_set_value(MACWI_REG_KEY* hKey, const char* lpValueName, uint32_t dwType, const uint8_t* lpData, uint32_t cbData);
macwi_status_t macwi_reg_query_value(MACWI_REG_KEY* hKey, const char* lpValueName, uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData);

// Save back to disk
macwi_status_t macwi_reg_save_to_disk(void);

#ifdef __cplusplus
}
#endif
