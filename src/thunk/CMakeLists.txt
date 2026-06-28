# ==============================================================================
# MacWI — Thunk module (32↔64 bit marshaling)
# ==============================================================================
# SPDX-License-Identifier: MIT

add_library(macwi_thunk STATIC
    thunk.c
)

target_include_directories(macwi_thunk PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
