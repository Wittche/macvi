/**
 * @file registry.c
 * @brief Registry emulation with JSON file persistence.
 *
 * The registry tree is stored in-memory and saved/loaded as JSON
 * files under ~/.macwi/registry/.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* ============================================================================
 * Internal data structures
 * ============================================================================ */

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
static RegKey* g_root_hkcr = NULL;
static RegKey* g_root_hku  = NULL;
static pthread_mutex_t g_reg_mutex = PTHREAD_MUTEX_INITIALIZER;
static char g_registry_dir[512] = {0};
static int g_dirty = 0;

/* ============================================================================
 * Helpers
 * ============================================================================ */

static RegKey* create_key_node(const char* name) {
    RegKey* k = (RegKey*)calloc(1, sizeof(RegKey));
    if (k) {
        k->name = strdup(name ? name : "");
    }
    return k;
}

static void ensure_dir(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        mkdir(path, 0755);
    }
}

static void get_registry_path(char* out, size_t out_size) {
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(out, out_size, "%s/.macwi/registry", home);
}

/* ============================================================================
 * JSON Serialization (Simple hand-rolled)
 * ============================================================================ */

static void write_indent(FILE* f, int depth) {
    for (int i = 0; i < depth; i++) fprintf(f, "  ");
}

static void escape_json_string(FILE* f, const char* s) {
    fputc('"', f);
    for (const char* p = s; *p; p++) {
        switch (*p) {
            case '"':  fprintf(f, "\\\""); break;
            case '\\': fprintf(f, "\\\\"); break;
            case '\n': fprintf(f, "\\n");  break;
            case '\r': fprintf(f, "\\r");  break;
            case '\t': fprintf(f, "\\t");  break;
            default:   fputc(*p, f);       break;
        }
    }
    fputc('"', f);
}

static void write_value_json(FILE* f, RegValue* v, int depth) {
    write_indent(f, depth);
    fprintf(f, "{\n");
    write_indent(f, depth + 1);
    fprintf(f, "\"name\": ");
    escape_json_string(f, v->name);
    fprintf(f, ",\n");
    write_indent(f, depth + 1);
    fprintf(f, "\"type\": %u,\n", v->type);
    write_indent(f, depth + 1);
    fprintf(f, "\"data\": \"");
    // Base64-like hex encoding for binary data
    for (uint32_t i = 0; i < v->data_len; i++) {
        fprintf(f, "%02x", v->data[i]);
    }
    fprintf(f, "\",\n");
    write_indent(f, depth + 1);
    fprintf(f, "\"data_len\": %u\n", v->data_len);
    write_indent(f, depth);
    fprintf(f, "}");
}

static void write_key_json(FILE* f, RegKey* k, int depth) {
    write_indent(f, depth);
    fprintf(f, "{\n");
    write_indent(f, depth + 1);
    fprintf(f, "\"name\": ");
    escape_json_string(f, k->name);
    fprintf(f, ",\n");

    // Values
    write_indent(f, depth + 1);
    fprintf(f, "\"values\": [");
    RegValue* v = k->values;
    int first = 1;
    while (v) {
        if (!first) fprintf(f, ",");
        fprintf(f, "\n");
        write_value_json(f, v, depth + 2);
        first = 0;
        v = v->next;
    }
    if (!first) {
        fprintf(f, "\n");
        write_indent(f, depth + 1);
    }
    fprintf(f, "],\n");

    // Subkeys
    write_indent(f, depth + 1);
    fprintf(f, "\"subkeys\": [");
    RegKey* sub = k->subkeys;
    first = 1;
    while (sub) {
        if (!first) fprintf(f, ",");
        fprintf(f, "\n");
        write_key_json(f, sub, depth + 2);
        first = 0;
        sub = sub->next;
    }
    if (!first) {
        fprintf(f, "\n");
        write_indent(f, depth + 1);
    }
    fprintf(f, "]\n");

    write_indent(f, depth);
    fprintf(f, "}");
}

static void save_hive(RegKey* root, const char* filename) {
    if (!root) return;
    char path[600];
    snprintf(path, sizeof(path), "%s/%s", g_registry_dir, filename);
    FILE* f = fopen(path, "w");
    if (!f) return;
    write_key_json(f, root, 0);
    fprintf(f, "\n");
    fclose(f);
}

/* ============================================================================
 * JSON Deserialization (Simple hand-rolled parser)
 * ============================================================================ */

static char* read_file_to_string(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char* parse_string(const char* p, char* out, size_t max_len) {
    p = skip_ws(p);
    if (*p != '"') return NULL;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < max_len - 1) {
        if (*p == '\\') {
            p++;
            switch (*p) {
                case '"':  out[i++] = '"';  break;
                case '\\': out[i++] = '\\'; break;
                case 'n':  out[i++] = '\n'; break;
                case 'r':  out[i++] = '\r'; break;
                case 't':  out[i++] = '\t'; break;
                default:   out[i++] = *p;   break;
            }
        } else {
            out[i++] = *p;
        }
        p++;
    }
    out[i] = '\0';
    if (*p == '"') p++;
    return p;
}

static const char* parse_uint(const char* p, uint32_t* out) {
    p = skip_ws(p);
    *out = 0;
    while (*p >= '0' && *p <= '9') {
        *out = (*out) * 10 + (*p - '0');
        p++;
    }
    return p;
}

static uint8_t hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return 0;
}

static const char* parse_hex_data(const char* p, uint8_t** out_data, uint32_t* out_len) {
    p = skip_ws(p);
    if (*p != '"') return NULL;
    p++;
    // Count hex chars
    const char* start = p;
    while (*p && *p != '"') p++;
    size_t hex_len = p - start;
    *out_len = (uint32_t)(hex_len / 2);
    *out_data = (uint8_t*)malloc(*out_len > 0 ? *out_len : 1);
    for (uint32_t i = 0; i < *out_len; i++) {
        (*out_data)[i] = (hex_nibble(start[i * 2]) << 4) | hex_nibble(start[i * 2 + 1]);
    }
    if (*p == '"') p++;
    return p;
}

// Forward declaration
static const char* parse_key_json(const char* p, RegKey** out_key);

static const char* find_field(const char* p, const char* field_name) {
    // Advance past the field_name colon
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", field_name);
    const char* found = strstr(p, search);
    if (!found) return NULL;
    found += strlen(search);
    found = skip_ws(found);
    if (*found == ':') found++;
    return skip_ws(found);
}

static const char* parse_value_json(const char* p, RegValue** out_val) {
    p = skip_ws(p);
    if (*p != '{') return NULL;
    
    // Find the closing brace for this value object
    int depth_counter = 1;
    const char* obj_start = p;
    const char* obj_end = p + 1;
    while (*obj_end && depth_counter > 0) {
        if (*obj_end == '{') depth_counter++;
        if (*obj_end == '}') depth_counter--;
        if (depth_counter > 0) obj_end++;
    }
    
    RegValue* v = (RegValue*)calloc(1, sizeof(RegValue));
    
    // Parse name
    char name[256] = {0};
    const char* f = find_field(obj_start, "name");
    if (f) parse_string(f, name, sizeof(name));
    v->name = strdup(name);
    
    // Parse type
    f = find_field(obj_start, "type");
    if (f) parse_uint(f, &v->type);
    
    // Parse data
    f = find_field(obj_start, "data");
    if (f) parse_hex_data(f, &v->data, &v->data_len);
    
    // Parse data_len (override if present)
    uint32_t explicit_len = 0;
    f = find_field(obj_start, "data_len");
    if (f) {
        parse_uint(f, &explicit_len);
        if (explicit_len > 0 && explicit_len != v->data_len) {
            // Trust data_len field
            v->data_len = explicit_len;
        }
    }
    
    *out_val = v;
    return obj_end + 1; // Past the closing '}'
}

static const char* parse_key_json(const char* p, RegKey** out_key) {
    p = skip_ws(p);
    if (*p != '{') return NULL;
    
    // Find closing brace for this key object (respecting nesting)
    int depth_counter = 1;
    const char* obj_start = p;
    const char* obj_end = p + 1;
    while (*obj_end && depth_counter > 0) {
        if (*obj_end == '{') depth_counter++;
        if (*obj_end == '}') depth_counter--;
        if (*obj_end == '"') {
            obj_end++;
            while (*obj_end && *obj_end != '"') {
                if (*obj_end == '\\') obj_end++;
                obj_end++;
            }
        }
        if (depth_counter > 0) obj_end++;
    }
    
    RegKey* k = (RegKey*)calloc(1, sizeof(RegKey));
    
    // Parse name
    char name[256] = {0};
    const char* f = find_field(obj_start, "name");
    if (f) parse_string(f, name, sizeof(name));
    k->name = strdup(name);
    
    // Parse values array
    f = find_field(obj_start, "values");
    if (f && *f == '[') {
        f++; // past '['
        f = skip_ws(f);
        RegValue* last_val = NULL;
        while (*f && *f != ']') {
            f = skip_ws(f);
            if (*f == ',') { f++; f = skip_ws(f); }
            if (*f == ']') break;
            RegValue* v = NULL;
            f = parse_value_json(f, &v);
            if (!f || !v) break;
            if (!k->values) {
                k->values = v;
            } else {
                last_val->next = v;
            }
            last_val = v;
            f = skip_ws(f);
            if (*f == ',') f++;
        }
        if (*f == ']') f++;
    }
    
    // Parse subkeys array
    f = find_field(obj_start, "subkeys");
    if (f && *f == '[') {
        f++; // past '['
        f = skip_ws(f);
        RegKey* last_sub = NULL;
        while (*f && *f != ']') {
            f = skip_ws(f);
            if (*f == ',') { f++; f = skip_ws(f); }
            if (*f == ']') break;
            RegKey* sub = NULL;
            f = parse_key_json(f, &sub);
            if (!f || !sub) break;
            if (!k->subkeys) {
                k->subkeys = sub;
            } else {
                last_sub->next = sub;
            }
            last_sub = sub;
            f = skip_ws(f);
            if (*f == ',') f++;
        }
    }
    
    *out_key = k;
    return obj_end + 1;
}

static RegKey* load_hive(const char* filename) {
    char path[600];
    snprintf(path, sizeof(path), "%s/%s", g_registry_dir, filename);
    char* json = read_file_to_string(path);
    if (!json) return NULL;
    
    RegKey* k = NULL;
    parse_key_json(json, &k);
    free(json);
    return k;
}

/* ============================================================================
 * Init / Save / Shutdown
 * ============================================================================ */

macwi_status_t macwi_registry_init(void) {
    pthread_mutex_lock(&g_reg_mutex);
    
    get_registry_path(g_registry_dir, sizeof(g_registry_dir));
    
    // Ensure directory hierarchy
    char base[512];
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(base, sizeof(base), "%s/.macwi", home);
    ensure_dir(base);
    ensure_dir(g_registry_dir);
    
    // Try to load from files
    g_root_hklm = load_hive("hklm.json");
    g_root_hkcu = load_hive("hkcu.json");
    g_root_hkcr = load_hive("hkcr.json");
    g_root_hku  = load_hive("hku.json");
    
    // Create root nodes if files didn't exist
    if (!g_root_hklm) g_root_hklm = create_key_node("HKEY_LOCAL_MACHINE");
    if (!g_root_hkcu) g_root_hkcu = create_key_node("HKEY_CURRENT_USER");
    if (!g_root_hkcr) g_root_hkcr = create_key_node("HKEY_CLASSES_ROOT");
    if (!g_root_hku)  g_root_hku  = create_key_node("HKEY_USERS");
    
    fprintf(stderr, "[macwi:registry] Loaded from %s\n", g_registry_dir);
    
    g_dirty = 0;
    pthread_mutex_unlock(&g_reg_mutex);
    return MACWI_SUCCESS;
}

void macwi_registry_save(void) {
    pthread_mutex_lock(&g_reg_mutex);
    if (g_dirty) {
        save_hive(g_root_hklm, "hklm.json");
        save_hive(g_root_hkcu, "hkcu.json");
        save_hive(g_root_hkcr, "hkcr.json");
        save_hive(g_root_hku,  "hku.json");
        fprintf(stderr, "[macwi:registry] Saved to %s\n", g_registry_dir);
        g_dirty = 0;
    }
    pthread_mutex_unlock(&g_reg_mutex);
}

/* ============================================================================
 * Navigation
 * ============================================================================ */

static RegKey* get_root_key(uint32_t hKey) {
    if (hKey == HKEY_LOCAL_MACHINE) return g_root_hklm;
    if (hKey == HKEY_CURRENT_USER)  return g_root_hkcu;
    if (hKey == HKEY_CLASSES_ROOT)  return g_root_hkcr;
    if (hKey == HKEY_USERS)         return g_root_hku;
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
    char* saveptr = NULL;
    char* token = strtok_r(path_copy, "\\/", &saveptr);
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
        token = strtok_r(NULL, "\\/", &saveptr);
    }

    free(path_copy);
    return current;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

macwi_status_t macwi_reg_open_key(uint32_t hKey, const char* lpSubKey, void** out_key_obj) {
    pthread_mutex_lock(&g_reg_mutex);

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
    g_dirty = 1;
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
    v->data = (uint8_t*)malloc(cbData > 0 ? cbData : 1);
    if (cbData > 0 && lpData) {
        memcpy(v->data, lpData, cbData);
    }

    g_dirty = 1;
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

macwi_status_t macwi_reg_delete_value(void* key_obj, const char* lpValueName) {
    if (!key_obj) return MACWI_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_reg_mutex);

    RegKey* k = (RegKey*)key_obj;
    const char* vname = lpValueName ? lpValueName : "";

    RegValue* prev = NULL;
    RegValue* v = k->values;
    while (v) {
        if (strcasecmp(v->name, vname) == 0) {
            if (prev) prev->next = v->next;
            else k->values = v->next;
            free(v->name);
            free(v->data);
            free(v);
            g_dirty = 1;
            pthread_mutex_unlock(&g_reg_mutex);
            return MACWI_SUCCESS;
        }
        prev = v;
        v = v->next;
    }

    pthread_mutex_unlock(&g_reg_mutex);
    return MACWI_ERROR_NOT_FOUND;
}

static void free_key_recursive(RegKey* k) {
    if (!k) return;
    // Free values
    RegValue* v = k->values;
    while (v) {
        RegValue* next_v = v->next;
        free(v->name);
        free(v->data);
        free(v);
        v = next_v;
    }
    // Free subkeys
    RegKey* sub = k->subkeys;
    while (sub) {
        RegKey* next_sub = sub->next;
        free_key_recursive(sub);
        sub = next_sub;
    }
    free(k->name);
    free(k);
}

macwi_status_t macwi_reg_delete_key(uint32_t hKey, const char* lpSubKey) {
    pthread_mutex_lock(&g_reg_mutex);

    RegKey* base = (hKey & 0x80000000) ? get_root_key(hKey) : (RegKey*)(uintptr_t)hKey;
    if (!base || !lpSubKey) {
        pthread_mutex_unlock(&g_reg_mutex);
        return MACWI_ERROR_INVALID_PARAM;
    }

    // Navigate to parent
    char* path_copy = strdup(lpSubKey);
    char* last_sep = strrchr(path_copy, '\\');
    if (!last_sep) last_sep = strrchr(path_copy, '/');

    RegKey* parent = base;
    const char* child_name = lpSubKey;

    if (last_sep) {
        *last_sep = '\0';
        parent = navigate_path(base, path_copy, 0);
        child_name = last_sep + 1;
    }
    free(path_copy);

    if (!parent) {
        pthread_mutex_unlock(&g_reg_mutex);
        return MACWI_ERROR_NOT_FOUND;
    }

    // Find and unlink the child
    RegKey* prev = NULL;
    RegKey* k = parent->subkeys;
    while (k) {
        if (strcasecmp(k->name, child_name) == 0) {
            if (prev) prev->next = k->next;
            else parent->subkeys = k->next;
            free_key_recursive(k);
            g_dirty = 1;
            pthread_mutex_unlock(&g_reg_mutex);
            return MACWI_SUCCESS;
        }
        prev = k;
        k = k->next;
    }

    pthread_mutex_unlock(&g_reg_mutex);
    return MACWI_ERROR_NOT_FOUND;
}

macwi_status_t macwi_reg_enum_key(void* key_obj, uint32_t dwIndex, char* lpName, uint32_t* lpcchName) {
    if (!key_obj || !lpName || !lpcchName) return MACWI_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_reg_mutex);

    RegKey* k = (RegKey*)key_obj;
    RegKey* sub = k->subkeys;
    uint32_t i = 0;

    while (sub && i < dwIndex) {
        sub = sub->next;
        i++;
    }

    if (!sub) {
        pthread_mutex_unlock(&g_reg_mutex);
        return MACWI_ERROR_NOT_FOUND; // ERROR_NO_MORE_ITEMS
    }

    size_t name_len = strlen(sub->name);
    if (name_len >= *lpcchName) {
        *lpcchName = (uint32_t)(name_len + 1);
        pthread_mutex_unlock(&g_reg_mutex);
        return MACWI_ERROR_MEMORY; // ERROR_MORE_DATA
    }

    strncpy(lpName, sub->name, *lpcchName);
    *lpcchName = (uint32_t)name_len;
    
    pthread_mutex_unlock(&g_reg_mutex);
    return MACWI_SUCCESS;
}

macwi_status_t macwi_reg_enum_value(void* key_obj, uint32_t dwIndex, char* lpValueName, uint32_t* lpcchValueName, uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData) {
    if (!key_obj || !lpValueName || !lpcchValueName) return MACWI_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_reg_mutex);

    RegKey* k = (RegKey*)key_obj;
    RegValue* v = k->values;
    uint32_t i = 0;

    while (v && i < dwIndex) {
        v = v->next;
        i++;
    }

    if (!v) {
        pthread_mutex_unlock(&g_reg_mutex);
        return MACWI_ERROR_NOT_FOUND; // ERROR_NO_MORE_ITEMS
    }

    size_t name_len = strlen(v->name);
    if (name_len >= *lpcchValueName) {
        *lpcchValueName = (uint32_t)(name_len + 1);
        pthread_mutex_unlock(&g_reg_mutex);
        return MACWI_ERROR_MEMORY;
    }

    strncpy(lpValueName, v->name, *lpcchValueName);
    *lpcchValueName = (uint32_t)name_len;

    if (lpType) *lpType = v->type;
    if (lpData && lpcbData && *lpcbData >= v->data_len) {
        memcpy(lpData, v->data, v->data_len);
    }
    if (lpcbData) *lpcbData = v->data_len;

    pthread_mutex_unlock(&g_reg_mutex);
    return MACWI_SUCCESS;
}
