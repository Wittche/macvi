cmake_minimum_required(VERSION 3.14)

project(FEXInterpreter_macOS
  VERSION 0.01
  LANGUAGES CXX)

set(SRCS
  FEXInterpreter_macOS.cpp
)

add_executable(FEXInterpreter_macOS ${SRCS})

target_include_directories(FEXInterpreter_macOS PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/)
target_include_directories(FEXInterpreter_macOS PRIVATE "${PROJECT_SOURCE_DIR}/Source/Tools/MacOSEmulation")

target_link_libraries(FEXInterpreter_macOS PRIVATE
  MacOSEmulation
  FEXCore
  Common
  JemallocLibs
)

# Mac OS Specific dependencies
find_library(FOUNDATION_FRAMEWORK Foundation)
target_link_libraries(FEXInterpreter_macOS PRIVATE ${FOUNDATION_FRAMEWORK})
