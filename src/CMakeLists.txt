# ==============================================================================
# MacWI — Main src build configuration
# ==============================================================================
# SPDX-License-Identifier: MIT

# Build subsystem libraries
add_subdirectory(core)
add_subdirectory(emu)
add_subdirectory(loader)
add_subdirectory(thunk)
add_subdirectory(win32)

# Build the main executable
add_executable(macwi main.c)

target_link_libraries(macwi
    PRIVATE
    macwi_loader
    macwi_thunk
    macwi_win32
    macwi_core
    macwi_emu
    FEXCore
    JemallocLibs
)
