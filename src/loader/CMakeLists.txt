# ==============================================================================
# MacWI — Loader module (pe_parser + pe_loader)
# ==============================================================================
# SPDX-License-Identifier: MIT

add_library(macwi_loader STATIC
    pe_parser.c
    pe_loader.c
)

target_include_directories(macwi_loader PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
