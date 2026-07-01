# ==============================================================================
# MacWI — Core module (handle table, shared infrastructure)
# ==============================================================================
# SPDX-License-Identifier: MIT

add_library(macwi_core STATIC
    handle.c
    vfs.c
    registry.c
    thread.c
    moltenvk_loader.c
)

target_include_directories(macwi_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
