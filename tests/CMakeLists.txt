# ==============================================================================
# MacWI — Test build configuration
# ==============================================================================
# SPDX-License-Identifier: MIT

enable_testing()

# PE parser test
add_executable(test_pe_parser test_pe_parser.c)

target_link_libraries(test_pe_parser PRIVATE
    macwi_loader
)

# Register with CTest
add_test(NAME pe_parser_tests COMMAND test_pe_parser)

# Basic Emulation test
add_executable(test_emu_basic test_emu_basic.c)
target_link_libraries(test_emu_basic PRIVATE
    macwi_emu
    macwi_core
)
add_test(NAME emu_basic_tests COMMAND test_emu_basic)

# PE Generator
add_executable(create_test_pe create_test_pe.c)

# PE Emulation test
add_executable(test_pe_emu test_pe_emu.c)
target_link_libraries(test_pe_emu PRIVATE
    macwi_loader
    macwi_emu
    macwi_core
)
add_test(NAME pe_emu_tests COMMAND test_pe_emu)

# Thunk Dispatch test
add_executable(test_thunk_dispatch test_thunk_dispatch.c)
target_link_libraries(test_thunk_dispatch PRIVATE
    macwi_thunk
    macwi_emu
    macwi_core
)
add_test(NAME thunk_dispatch_tests COMMAND test_thunk_dispatch)
