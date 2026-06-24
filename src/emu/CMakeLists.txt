# ==============================================================================
# MacWI — Emulation Module
# ==============================================================================
# SPDX-License-Identifier: MIT

add_library(macwi_emu STATIC
    emu.c
)

# Link against the Unicorn Engine target imported via FetchContent
target_link_libraries(macwi_emu PUBLIC unicorn)
