/**
 * @file gdi32.h
 * @brief GDI32 (Graphics Device Interface) stub implementations.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "macwi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register all GDI32 APIs.
 */
void macwi_gdi32_register_apis(void);

#ifdef __cplusplus
}
#endif
