cmake_minimum_required(VERSION 3.14)
project(FEX C CXX ASM)

include(CheckIncludeFiles)
check_include_files("gdb/jit-reader.h" HAVE_GDB_JIT_READER_H)

option(ENABLE_IWYU "Enable the Include What You Use sanitizer" FALSE)
option(ENABLE_LTO "Enable LTO with compilation" TRUE)
option(ENABLE_XRAY "Enable building with LLVM X-Ray" FALSE)
set(USE_LINKER "" CACHE STRING "Path to a custom linker program")
option(ENABLE_UBSAN "Enable the Clang Undefined Behavior Sanitizer" FALSE)
option(ENABLE_ASAN "Enable the Clang Address Sanitizer" FALSE)
option(ENABLE_TSAN "Enable the Clang Thread Sanitizer" FALSE)
option(ENABLE_COVERAGE "Enable Code Coverage" FALSE)
option(ENABLE_ASSERTIONS "Enable debug assertions" FALSE)
option(ENABLE_STRICT_WERROR "Enable stricter -Werror" FALSE)
option(ENABLE_WERROR "Enable -Werror" FALSE)
option(ENABLE_FEX_ALLOCATOR "Enable allocator for FEX" TRUE)
option(ENABLE_OFFLINE_TELEMETRY "Enable FEX offline telemetry" TRUE)
option(ENABLE_COMPILE_TIME_TRACE "Enable time trace compile option" FALSE)
option(ENABLE_LIBCXX "Use LLVM's libc++ instead of the GNU libstdc++" FALSE)
option(ENABLE_CCACHE "Enable ccache for build caching" TRUE)
option(ENABLE_VIXL_SIMULATOR "Use the VIXL simulator for emulation (only useful for CI testing)" FALSE)
option(ENABLE_VIXL_DISASSEMBLER "Enable debug disassembler output with VIXL" FALSE)
option(ENABLE_ZYDIS "Enable x86/x86-64 guest disassembler output with Zydis" FALSE)
option(ENABLE_FEXCORE_PROFILER "Enable FEXCore's timeline profiling capabilities" FALSE)
set(FEXCORE_PROFILER_BACKEND "gpuvis" CACHE STRING "Set which backend to use for FEXCore's profiler")
set_property(CACHE FEXCORE_PROFILER_BACKEND PROPERTY STRINGS gpuvis tracy)
option(USE_PDB_DEBUGINFO "Build debug info in PDB format" FALSE)

set(X86_32_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Data/CMake/toolchain_x86_32.cmake" CACHE FILEPATH "Toolchain file for the (cross-)compiler targeting i686")
if (APPLE)
  set(X86_64_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Data/CMake/toolchain_macos_native_thunks.cmake" CACHE FILEPATH "Toolchain file for the (cross-)compiler targeting x86_64" FORCE)
  message(STATUS "macOS: Using native thunking toolchain")
endif()
set(DATA_DIRECTORY "" CACHE PATH "Global data directory (override)")
set(HOSTLIBS_DATA_DIRECTORY "" CACHE PATH "Global data directory (override)")

if (NOT DATA_DIRECTORY)
  set(DATA_DIRECTORY "${CMAKE_INSTALL_PREFIX}/share/fex-emu")
endif()

include(GNUInstallDirs)
if (NOT HOSTLIBS_DATA_DIRECTORY)
  set(HOSTLIBS_DATA_DIRECTORY "${CMAKE_INSTALL_FULL_LIBDIR}/fex-emu")
endif()

## Platform Checks ##
if (NOT APPLE)
  message(FATAL_ERROR "This version of FEX only supports macOS.")
endif()

## Compiler Checks ##
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  message(FATAL_ERROR "FEX doesn't support GCC! Use Clang instead.")
endif()
message(STATUS "Building for Apple")

## Architecture Handling ##
string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} processor)
if (processor MATCHES "^aarch64|^arm64|^armv8\.*")
  set(ARCHITECTURE_arm64 1)
  add_compile_definitions(ARCHITECTURE_arm64=1)
else()
  message(FATAL_ERROR "Unsupported processor type ${processor}. FEX-macOS requires ARM64.")
endif()

if (ENABLE_FEXCORE_PROFILER)
  add_compile_definitions(ENABLE_FEXCORE_PROFILER=1)
  string(TOUPPER "${FEXCORE_PROFILER_BACKEND}" FEXCORE_PROFILER_BACKEND)

  if (FEXCORE_PROFILER_BACKEND STREQUAL "GPUVIS")
    add_compile_definitions(FEXCORE_PROFILER_BACKEND=1)
  elseif (FEXCORE_PROFILER_BACKEND STREQUAL "TRACY")
    add_compile_definitions(FEXCORE_PROFILER_BACKEND=2)
    add_compile_definitions(TRACY_ENABLE=1)
    add_compile_definitions(TRACY_MANUAL_LIFETIME=1)
    add_compile_definitions(TRACY_DELAYED_INIT=1)
    add_compile_definitions(TRACY_NO_CRASH_HANDLER=1)
    add_compile_definitions(TRACY_NO_SAMPLING=1)
    add_compile_definitions(TRACY_NO_CALLSTACK=1)
  else()
    message(FATAL_ERROR "Unknown FEXCore profiler backend ${FEXCORE_PROFILER_BACKEND}")
  endif()
endif()

# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Data/CMake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/cmake_uninstall.cmake)
endif()

set(TUNE_CPU "native" CACHE STRING "Override the CPU the build is tuned for")
set(TUNE_ARCH "generic" CACHE STRING "Override the Arch the build is tuned for")
set(OVERRIDE_VERSION "detect" CACHE STRING "Override the FEX version")
set(OVERRIDE_HASH "detect" CACHE STRING "Override the FEX git hash")

get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release
        CACHE STRING "Choose the type of build." FORCE)
    message(STATUS "No build type set, defaulting to a Release build")
endif()

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)
if (CMAKE_BUILD_TYPE MATCHES "DEBUG")
  set(ENABLE_ASSERTIONS TRUE)
endif()

if (ENABLE_ASSERTIONS)
  message(STATUS "Assertions enabled")
  add_compile_definitions(ASSERTIONS_ENABLED=1)
endif()

add_compile_definitions(_LARGEFILE64_SOURCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Bin)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
cmake_policy(SET CMP0083 NEW) # Follow new PIE policy
include(CheckPIESupported)
check_pie_supported()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ${ENABLE_LTO})

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-std=c++11 -Wattributes -Werror=attributes")
check_cxx_source_compiles(
  "
  __attribute__((preserve_all))
  int Testy(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
  }
  int main() {
  return Testy(0, 1, 2, 3, 4, 5);
  }"
  HAS_CLANG_PRESERVE_ALL)
unset(CMAKE_REQUIRED_FLAGS)
if (HAS_CLANG_PRESERVE_ALL)
  message(STATUS "Has clang::preserve_all")
endif()

if (ARCHITECTURE_arm64 AND HAS_CLANG_PRESERVE_ALL)
  add_compile_definitions("FEX_PRESERVE_ALL_ATTR=__attribute__((preserve_all))" "FEX_HAS_PRESERVE_ALL_ATTR=1")
else()
  add_compile_definitions("FEX_PRESERVE_ALL_ATTR=" "FEX_HAS_PRESERVE_ALL_ATTR=0")
endif()

if (ENABLE_VIXL_SIMULATOR)
  add_compile_definitions(VIXL_SIMULATOR=1 VIXL_INCLUDE_SIMULATOR_AARCH64=1)
endif()

if (ENABLE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    message(STATUS "CCache enabled")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
  endif()
endif()

if (ENABLE_XRAY)
  add_compile_options(-fxray-instrument)
  link_libraries(-fxray-instrument)
endif()

if (ENABLE_COMPILE_TIME_TRACE)
  add_compile_options(-ftime-trace)
  link_libraries(-ftime-trace)
endif()

set(PTHREAD_LIB pthread)

if (USE_LINKER)
  message(STATUS "Overriding linker to: ${USE_LINKER}")
  add_link_options("-fuse-ld=${USE_LINKER}")
endif()

if (ENABLE_LIBCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
endif()

if (NOT ENABLE_OFFLINE_TELEMETRY)
  add_compile_definitions(FEX_DISABLE_TELEMETRY=1)
endif()

if (ENABLE_UBSAN)
  add_compile_definitions(ENABLE_UBSAN=1)
  add_compile_options(-fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize=alignment -fno-sanitize=function -fno-sanitize-recover=undefined)
  link_libraries(-fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize=alignment -fno-sanitize=function -fno-sanitize-recover=undefined)
endif()

if (ENABLE_ASAN)
  add_compile_definitions(ENABLE_ASAN=1)
  add_compile_options(-fno-omit-frame-pointer -fsanitize=address -fsanitize-address-use-after-scope)
  link_libraries(-fno-omit-frame-pointer -fsanitize=address -fsanitize-address-use-after-scope)
endif()

if (ENABLE_TSAN)
  add_compile_options(-fno-omit-frame-pointer -fsanitize=thread)
  link_libraries(-fno-omit-frame-pointer -fsanitize=thread)
endif()

if (ENABLE_COVERAGE)
  add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
  link_libraries(-fprofile-instr-generate -fcoverage-mapping)
endif()

if (ENABLE_FEX_ALLOCATOR)
  add_subdirectory(External/rpmalloc/)
endif()

if (USE_PDB_DEBUGINFO)
  add_compile_options(-g -gcodeview)
  add_link_options(-g -Wl,--pdb=)
endif()

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")
set(CMAKE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_LINKER_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fomit-frame-pointer")
set(CMAKE_LINKER_FLAGS_RELEASE "${CMAKE_LINKER_FLAGS_RELEASE} -fomit-frame-pointer")

## Modules ##
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/Data/CMake/)

include(LinkerGC)

## Externals ##

find_package(unordered_dense QUIET CONFIG)
if (NOT unordered_dense_FOUND)
  add_subdirectory(External/unordered_dense)
endif()

include(CTest)
if (BUILD_TESTING OR ENABLE_VIXL_DISASSEMBLER OR ENABLE_VIXL_SIMULATOR)
  add_subdirectory(External/vixl/)
endif()

if (ENABLE_ZYDIS)
  find_package(Zycore 1.5 MODULE QUIET)
  find_package(Zydis 4.0 MODULE QUIET)

  if (TARGET Zydis::Zydis AND TARGET Zycore::Zycore)
    message(STATUS "Using system Zydis")
  else()
    set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    message(STATUS "Using bundled Zydis")
    add_subdirectory(External/zydis/)
  endif()
endif()

if (ENABLE_FEXCORE_PROFILER AND FEXCORE_PROFILER_BACKEND STREQUAL "TRACY")
  add_subdirectory(External/tracy)
endif()

find_package(Python 3.9 REQUIRED COMPONENTS Interpreter)

set(BUILD_SHARED_LIBS OFF)

if (NOT TARGET xxHash::xxhash)
  set(XXHASH_BUNDLED_MODE TRUE)
  set(XXHASH_BUILD_XXHSUM FALSE)
  add_subdirectory(External/xxhash/cmake_unofficial/)
endif()

add_compile_options(-Wno-trigraphs)
add_compile_definitions(GLOBAL_DATA_DIRECTORY="${DATA_DIRECTORY}/")

if (BUILD_TESTING)
  find_package(Catch2 3 QUIET)
  if (NOT Catch2_FOUND)
    add_subdirectory(External/Catch2/)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/External/Catch2/contrib/")
  endif()
  include(Catch)
endif()

find_package(fmt QUIET)
if (NOT fmt_FOUND)
  set(FMT_INSTALL OFF)
  add_subdirectory(External/fmt/)
endif()

find_package(range-v3 QUIET)
if (NOT range-v3_FOUND)
  add_subdirectory(External/range-v3/)
  target_compile_definitions(range-v3 INTERFACE RANGES_DISABLE_DEPRECATED_WARNINGS)
endif()

add_subdirectory(External/tiny-json/)

include_directories(Source/)
include_directories("${CMAKE_BINARY_DIR}/Source/")

include(CheckCXXCompilerFlag)

check_cxx_compiler_flag(-fdiagnostics-color=always GCC_COLOR)
check_cxx_compiler_flag(-fcolor-diagnostics CLANG_COLOR)
check_cxx_compiler_flag(-Wno-deprecated-enum-enum-conversion ENUM_ENUM_WARNING)

if (GCC_COLOR)
  add_compile_options(-fdiagnostics-color=always)
endif()
if (CLANG_COLOR)
  add_compile_options(-fcolor-diagnostics)
endif()

if(ENUM_ENUM_WARNING)
  add_compile_options(-Wno-deprecated-enum-enum-conversion)
endif()

if(ENABLE_WERROR OR ENABLE_STRICT_WERROR)
  add_compile_options(-Werror)
  if (NOT ENABLE_STRICT_WERROR)
    add_compile_options(-Wno-error=unused-variable)
  endif()
endif()

set(FEX_TUNE_COMPILE_FLAGS)
if (NOT TUNE_ARCH STREQUAL "generic")
  check_cxx_compiler_flag("-march=${TUNE_ARCH}" COMPILER_SUPPORTS_ARCH_TYPE)
  if(COMPILER_SUPPORTS_ARCH_TYPE)
    list(APPEND FEX_TUNE_COMPILE_FLAGS "-march=${TUNE_ARCH}")
  endif()
endif()

if (TUNE_CPU STREQUAL "native")
  set(AARCH64_CPU "apple-m1")
  check_cxx_compiler_flag("-mcpu=${AARCH64_CPU}" COMPILER_SUPPORTS_CPU_TYPE)
  if(COMPILER_SUPPORTS_CPU_TYPE)
    list(APPEND FEX_TUNE_COMPILE_FLAGS "-mcpu=${AARCH64_CPU}")
  endif()
endif()

set(GIT_DESCRIBE_STRING "FEX-Unknown")
find_package(Git)
if (GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --abbrev=7
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_DESCRIBE_STRING
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

set(GIT_HASH "Unknown")
if (GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_HASH
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

message(STATUS "FEX version: ${GIT_DESCRIBE_STRING}")
message(STATUS "FEX commit: ${GIT_HASH}")

string(REGEX
  REPLACE "(..|.$)" "0x\\1, "
  GIT_HASH_ARRAY "${GIT_HASH}")

if (ENABLE_IWYU)
  find_program(IWYU_EXE NAMES iwyu include-what-you-use)
  if (IWYU_EXE)
    message(STATUS "IWYU enabled")
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU_EXE}")
  endif()
endif()

add_compile_options(-Wall)

add_subdirectory(External/SoftFloat-3e/)
add_subdirectory(External/cephes/)
add_subdirectory(FEXHeaderUtils/)
add_subdirectory(CodeEmitter/)
add_subdirectory(FEXCore/)

add_subdirectory(Source/)

if (BUILD_TESTING)
  add_subdirectory(unittests/)
endif()
