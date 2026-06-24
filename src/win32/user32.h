/**
 * @file user32.h
 * @brief Win32 user32.dll API registration.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register all user32.dll stub functions with the thunking dispatcher.
 */
void macwi_user32_register_apis(void);

#ifdef __cplusplus
}
#endif
