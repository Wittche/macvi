# ==============================================================================
# MacWI — Win32 API stubs (kernel32, ntdll)
# ==============================================================================
# SPDX-License-Identifier: MIT

add_library(macwi_win32 STATIC
    kernel32.c
    ntdll.c
    user32.c
    cocoa_bridge.m
)

target_include_directories(macwi_win32 PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(macwi_win32 PRIVATE
    macwi_core
)

# Enable Objective-C if needed implicitly, and link Cocoa
set_source_files_properties(cocoa_bridge.m PROPERTIES LANGUAGE OBJC)
target_link_options(macwi_win32 PUBLIC "-framework" "Cocoa")
