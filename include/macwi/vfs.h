/**
 * @file vfs.h
 * @brief Virtual File System (VFS) mapping Windows paths to POSIX.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "macwi/types.h"
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

// Max path length for internal POSIX paths
#define MACWI_MAX_PATH 4096

/**
 * Initialize the Virtual File System.
 * Ensures ~/.macwi/drive_c and essential Windows directories exist.
 */
macwi_status_t macwi_vfs_init(void);

/**
 * Convert a Windows DOS path (e.g. C:\Windows\System32\foo.dll) to a 
 * host POSIX path (e.g. /Users/name/.macwi/drive_c/windows/system32/foo.dll).
 * Handles case-insensitivity by searching the directory tree if exact match fails.
 *
 * @param dos_path Input Windows path.
 * @param unix_path Output buffer for the resolved POSIX path (must be MACWI_MAX_PATH size).
 * @return MACWI_SUCCESS if successful.
 */
macwi_status_t macwi_vfs_dos_to_unix(const char* dos_path, char* unix_path);

#ifdef __cplusplus
}
#endif
