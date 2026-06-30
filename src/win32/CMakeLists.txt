# ==============================================================================
# MacWI — Win32 API stubs (kernel32, ntdll)
# ==============================================================================
# SPDX-License-Identifier: MIT

add_library(macwi_win32 STATIC
    kernel32.c
    ntdll.c
    advapi32.c
    user32.c
    timer.c
    gdi32.c
    cocoa_window.m
)

target_include_directories(macwi_win32 PRIVATE 
    ${CMAKE_SOURCE_DIR}/include 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(macwi_win32 PRIVATE macwi_core macwi_thunk)
target_link_libraries(macwi_win32 PUBLIC "-framework Cocoa")

# Enable Objective-C for .m files
set_source_files_properties(cocoa_window.m PROPERTIES LANGUAGE OBJC)
