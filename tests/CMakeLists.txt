# ==============================================================================
# MacWI — Test build configuration
# ==============================================================================
# SPDX-License-Identifier: MIT

enable_testing()

# PE parser test
add_executable(test_pe_parser test_pe_parser.c)

target_link_libraries(test_pe_parser PRIVATE
    macwi_loader
    macwi_thunk
    macwi_emu
    macwi_core
    JemallocLibs
)

# Register with CTest
add_test(NAME pe_parser_tests COMMAND test_pe_parser)

# Basic Emulation test
add_executable(test_emu_basic test_emu_basic.c)
target_link_libraries(test_emu_basic PRIVATE
    macwi_emu
    macwi_thunk
    macwi_core
    JemallocLibs
)
add_test(NAME emu_basic_tests COMMAND test_emu_basic)



# PE Emulation test
add_executable(test_pe_emu test_pe_emu.c)
target_link_libraries(test_pe_emu PRIVATE
    macwi_loader
    macwi_emu
    macwi_thunk
    macwi_core
    JemallocLibs
)
add_test(NAME pe_emu_tests COMMAND test_pe_emu)

# Thunk Dispatch test
add_executable(test_thunk_dispatch test_thunk_dispatch.c)
target_link_libraries(test_thunk_dispatch PRIVATE
    macwi_thunk
    macwi_emu
    macwi_core
    JemallocLibs
)
add_test(NAME thunk_dispatch_tests COMMAND test_thunk_dispatch)

# Build hello_win32.exe using MinGW
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/hello_win32.exe
    COMMAND i686-w64-mingw32-gcc ${CMAKE_CURRENT_SOURCE_DIR}/hello_win32.c -o ${CMAKE_CURRENT_BINARY_DIR}/hello_win32.exe -nostdlib -lkernel32 -Wl,-e,__start -Wl,--image-base,0x28000000
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hello_win32.c
    COMMENT "Building 32-bit Windows PE executable with MinGW"
)
add_custom_target(hello_win32 ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/hello_win32.exe)

# Build advanced_win32.exe using MinGW
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/advanced_win32.exe
    COMMAND i686-w64-mingw32-gcc ${CMAKE_CURRENT_SOURCE_DIR}/advanced_win32.c -o ${CMAKE_CURRENT_BINARY_DIR}/advanced_win32.exe -nostdlib -lkernel32 -ladvapi32 -Wl,-e,__start -Wl,--image-base,0x30000000
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/advanced_win32.c
    COMMENT "Building advanced 32-bit Windows PE executable with MinGW"
)
add_custom_target(advanced_win32 ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/advanced_win32.exe)

# Build fs_win32.exe using MinGW
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fs_win32.exe
    COMMAND i686-w64-mingw32-gcc ${CMAKE_CURRENT_SOURCE_DIR}/fs_win32.c -o ${CMAKE_CURRENT_BINARY_DIR}/fs_win32.exe -nostdlib -lkernel32 -Wl,-e,__start -Wl,--image-base,0x29000000
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fs_win32.c
    COMMENT "Building File System Test 32-bit Windows PE executable with MinGW"
)
add_custom_target(fs_win32 ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fs_win32.exe)
