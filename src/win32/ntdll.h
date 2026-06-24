/**
 * @file ntdll.h
 * @brief Win32 ntdll.dll API registration.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register all ntdll.dll stub functions with the thunking dispatcher.
 */
void macwi_ntdll_register_apis(void);

#ifdef __cplusplus
}
#endif
