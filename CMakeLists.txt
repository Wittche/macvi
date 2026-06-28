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
    LANGUAGES C CXX OBJC
)

# ------------------------------------------------------------------------------
# Global compiler settings
# ------------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Enforce ARM64 target on macOS
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture" FORCE)
endif()

# Strict warnings for all targets
add_compile_options(-Wall -Wextra -Wpedantic)

# Debug-specific: sanitizers for development
    add_compile_options(-g -O0)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

# Allow FEXCore to find its CMake modules when included as a subdirectory
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/FEX/Data/CMake/)

# Disable standalone executables in FEX directory to only build FEXCore library
set(ENABLE_LTO OFF CACHE BOOL "" FORCE)
set(ENABLE_VIXL_SIMULATOR OFF CACHE BOOL "" FORCE)
set(ENABLE_LTO OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTS OFF CACHE BOOL "" FORCE)

# Include FEXCore (x86_64 to ARM64 Translator)
add_subdirectory(FEX)

# ------------------------------------------------------------------------------
# Include directories — public headers available to all targets
# ------------------------------------------------------------------------------
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/FEX/FEXCore/include
)

# ------------------------------------------------------------------------------
# Subdirectories
# ------------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(tests)
