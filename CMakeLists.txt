# ==============================================================================
# MacWI — Root CMake Build Configuration
# ==============================================================================
# WoW64-like compatibility layer for running 32-bit Windows applications
# on Apple Silicon macOS.
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.20)

project(macwi
    VERSION 0.1.0
    DESCRIPTION "WoW64-like compatibility layer for Apple Silicon macOS"
    LANGUAGES C OBJC
)

# ------------------------------------------------------------------------------
# Global compiler settings
# ------------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Enforce ARM64 target on macOS
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture" FORCE)
endif()

# Strict warnings for all targets
add_compile_options(-Wall -Wextra -Wpedantic)

# Debug-specific: sanitizers for development
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
include(FetchContent)

# Fetch Unicorn Engine (lightweight CPU emulator framework)
FetchContent_Declare(
    unicorn
    GIT_REPOSITORY https://github.com/unicorn-engine/unicorn.git
    GIT_TAG        master
)
set(UNICORN_ARCH "x86" CACHE STRING "" FORCE)
set(UNICORN_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(unicorn)

# ------------------------------------------------------------------------------
# Include directories — public headers available to all targets
# ------------------------------------------------------------------------------
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${unicorn_SOURCE_DIR}/include
)

# ------------------------------------------------------------------------------
# Subdirectories
# ------------------------------------------------------------------------------
add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

