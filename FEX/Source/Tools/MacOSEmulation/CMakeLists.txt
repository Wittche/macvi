cmake_minimum_required(VERSION 3.14)

project(MacOSEmulation
  VERSION 0.01
  LANGUAGES CXX)

set(SRCS
  MacOSSyscalls.cpp
  MachOLoader.cpp
  PELoader.cpp
  MetalBridge.mm
  MachException.cpp
  VFSEmulator.cpp
  RegistryEmulator.cpp
  Metal_D3D11.mm
  winemac/cocoa_app.m
  winemac/cocoa_clipboard.m
  winemac/cocoa_cursorclipping.m
  winemac/cocoa_display.m
  winemac/cocoa_event.m
  winemac/cocoa_main.m
  winemac/cocoa_opengl.m
  winemac/cocoa_status_item.m
  winemac/cocoa_window.m
)

add_library(MacOSEmulation STATIC ${SRCS})

target_include_directories(MacOSEmulation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/)
target_include_directories(MacOSEmulation PUBLIC "${PROJECT_SOURCE_DIR}/include/")
target_include_directories(MacOSEmulation PUBLIC "${CMAKE_BINARY_DIR}/include/")
target_include_directories(MacOSEmulation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/wine_include)
target_include_directories(MacOSEmulation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/winemac)

# Add Wine specific definitions so the headers compile correctly natively
target_compile_definitions(MacOSEmulation PRIVATE _REENTRANT __WINESRC__ WINE_UNIX_LIB)
target_compile_options(MacOSEmulation PRIVATE -Wno-deprecated-declarations -Wno-format)

target_link_libraries(MacOSEmulation PUBLIC FEXCore Common CommonTools FEXHeaderUtils)

# Add macOS specific frameworks
find_library(FOUNDATION_FRAMEWORK Foundation)
find_library(METAL_FRAMEWORK Metal)
find_library(QUARTZ_FRAMEWORK QuartzCore)
find_library(APPKIT_FRAMEWORK AppKit)
find_library(CARBON_FRAMEWORK Carbon)
find_library(OPENGL_FRAMEWORK OpenGL)
target_link_libraries(MacOSEmulation PRIVATE 
    ${FOUNDATION_FRAMEWORK}
    ${METAL_FRAMEWORK}
    ${QUARTZ_FRAMEWORK}
    ${APPKIT_FRAMEWORK}
    ${CARBON_FRAMEWORK}
    ${OPENGL_FRAMEWORK}
)
