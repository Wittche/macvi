cmake_minimum_required(VERSION 3.14)

project(FEXMacOS
  VERSION 0.01
  LANGUAGES CXX)

set(SRCS
  Main.mm
)

add_executable(FEXMacOS ${SRCS})

target_include_directories(FEXMacOS PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/)
target_include_directories(FEXMacOS PUBLIC "${PROJECT_SOURCE_DIR}/include/")
target_include_directories(FEXMacOS PUBLIC "${CMAKE_BINARY_DIR}/include/")

target_link_libraries(FEXMacOS PUBLIC FEXCore MacOSEmulation JemallocLibs Common)

# Add macOS specific frameworks
find_library(FOUNDATION_FRAMEWORK Foundation)
target_link_libraries(FEXMacOS PRIVATE ${FOUNDATION_FRAMEWORK})
