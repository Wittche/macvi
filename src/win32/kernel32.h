/**
 * @file kernel32.h
 * @brief Win32 kernel32.dll API registration.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register all kernel32.dll stub functions with the thunking dispatcher.
 */
void macwi_kernel32_register_apis(void);

#ifdef __cplusplus
}
#endif
