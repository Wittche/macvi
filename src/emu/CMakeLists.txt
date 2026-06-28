# ==============================================================================
# MacWI — Emulation Module
# ==============================================================================
# SPDX-License-Identifier: MIT

add_library(macwi_emu STATIC
    emu.cpp
)

# Link against the FEXCore target imported via add_subdirectory
target_link_libraries(macwi_emu PRIVATE FEXCore)

# FEXCore SyscallHandler depends on fmt
target_include_directories(macwi_emu PRIVATE
    ${CMAKE_SOURCE_DIR}/FEX/External/fmt/include
)
