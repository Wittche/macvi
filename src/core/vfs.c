/**
 * @file vfs.c
 * @brief VFS implementation for DOS to POSIX path translation.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <pthread.h>

static char g_drive_c_path[MACWI_MAX_PATH];
static pthread_mutex_t g_vfs_mutex = PTHREAD_MUTEX_INITIALIZER;

static void create_dir_if_not_exists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }
}

macwi_status_t macwi_vfs_init(void) {
    pthread_mutex_lock(&g_vfs_mutex);
    const char* home = getenv("HOME");
    if (!home) {
        pthread_mutex_unlock(&g_vfs_mutex);
        return MACWI_ERROR_IO;
    }
    
    // Create ~/.macwi
    snprintf(g_drive_c_path, sizeof(g_drive_c_path), "%s/.macwi", home);
    create_dir_if_not_exists(g_drive_c_path);
    
    // Create ~/.macwi/drive_c
    snprintf(g_drive_c_path, sizeof(g_drive_c_path), "%s/.macwi/drive_c", home);
    create_dir_if_not_exists(g_drive_c_path);
    
    // Create basic Windows structure
    char temp_path[MACWI_MAX_PATH];
    snprintf(temp_path, sizeof(temp_path), "%s/windows", g_drive_c_path);
    create_dir_if_not_exists(temp_path);
    
    snprintf(temp_path, sizeof(temp_path), "%s/windows/system32", g_drive_c_path);
    create_dir_if_not_exists(temp_path);
    
    pthread_mutex_unlock(&g_vfs_mutex);
    return MACWI_SUCCESS;
}

// Convert backslashes to forward slashes
static void normalize_slashes(char* path) {
    for (int i = 0; path[i]; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
}

static int case_insensitive_resolve(const char* base_dir, const char* name, char* out_name) {
    DIR* dir = opendir(base_dir);
    if (!dir) return 0;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcasecmp(entry->d_name, name) == 0) {
            strcpy(out_name, entry->d_name);
            closedir(dir);
            return 1;
        }
    }
    closedir(dir);
    return 0;
}

macwi_status_t macwi_vfs_dos_to_unix(const char* dos_path, char* unix_path) {
    if (!dos_path || !unix_path) return MACWI_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&g_vfs_mutex);
    
    char temp[MACWI_MAX_PATH];
    strncpy(temp, dos_path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    normalize_slashes(temp);
    
    // Check if it's an absolute DOS path starting with C:
    if ((temp[0] == 'C' || temp[0] == 'c') && temp[1] == ':' && temp[2] == '/') {
        char current_path[MACWI_MAX_PATH];
        strcpy(current_path, g_drive_c_path);
        
        char* token = strtok(temp + 3, "/");
        while (token != NULL) {
            char actual_name[256];
            if (case_insensitive_resolve(current_path, token, actual_name)) {
                strcat(current_path, "/");
                strcat(current_path, actual_name);
            } else {
                // If not found, just append the token as is (for creating new files)
                strcat(current_path, "/");
                strcat(current_path, token);
            }
            token = strtok(NULL, "/");
        }
        
        strcpy(unix_path, current_path);
        pthread_mutex_unlock(&g_vfs_mutex);
        return MACWI_SUCCESS;
    }
    
    // If it's relative or another drive, fallback to host CWD (simplification)
    char current_path[MACWI_MAX_PATH];
    if (getcwd(current_path, sizeof(current_path)) != NULL) {
        char* token = strtok(temp, "/");
        while (token != NULL) {
            char actual_name[256];
            if (case_insensitive_resolve(current_path, token, actual_name)) {
                strcat(current_path, "/");
                strcat(current_path, actual_name);
            } else {
                strcat(current_path, "/");
                strcat(current_path, token);
            }
            token = strtok(NULL, "/");
        }
        strcpy(unix_path, current_path);
    } else {
        strcpy(unix_path, temp);
    }
    
    pthread_mutex_unlock(&g_vfs_mutex);
    return MACWI_SUCCESS;
}
