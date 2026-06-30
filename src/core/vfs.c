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

macwi_status_t macwi_vfs_dos_to_unix(const char* dos_path, char* unix_path) {
    if (!dos_path || !unix_path) return MACWI_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&g_vfs_mutex);
    
    char temp[MACWI_MAX_PATH];
    strncpy(temp, dos_path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    normalize_slashes(temp);
    
    // Check if it's an absolute DOS path starting with C:
    if ((temp[0] == 'C' || temp[0] == 'c') && temp[1] == ':' && temp[2] == '/') {
        // Simple append for now. 
        // A true implementation needs case-insensitive directory traversal.
        snprintf(unix_path, MACWI_MAX_PATH, "%s/%s", g_drive_c_path, temp + 3);
        
        // Very basic case-insensitivity: lowercase the path
        // (In reality, we should opendir/readdir to match the actual file casing on APFS)
        for(size_t i = strlen(g_drive_c_path); unix_path[i]; i++) {
            unix_path[i] = tolower((unsigned char)unix_path[i]);
        }
        
        pthread_mutex_unlock(&g_vfs_mutex);
        return MACWI_SUCCESS;
    }
    
    // If it's relative or another drive, fallback to host CWD (simplification)
    strncpy(unix_path, temp, MACWI_MAX_PATH - 1);
    unix_path[MACWI_MAX_PATH - 1] = '\0';
    pthread_mutex_unlock(&g_vfs_mutex);
    return MACWI_SUCCESS;
}
