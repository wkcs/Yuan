cmake_minimum_required(VERSION 3.16)

# Include version configuration
include(cmake/Version.cmake)

project(yuan
    VERSION ${YUAN_VERSION_MAJOR}.${YUAN_VERSION_MINOR}.${YUAN_VERSION_PATCH}
    LANGUAGES CXX
    DESCRIPTION "Yuan Programming Language Compiler"
)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# Prebuilt LLVM on Windows is commonly built against static MSVC runtime.
# Default to static runtime to avoid MD/MT mismatch at link time.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "MSVC runtime library" FORCE)
endif()

# Options
option(YUAN_BUILD_TESTS "Build Yuan tests" ON)
option(YUAN_BUILD_DOCS "Build Yuan documentation" OFF)

# Find LLVM
include(cmake/FindLLVM.cmake)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR}/include)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# On Windows, prebuilt LLVM archives are often built as release artifacts
# (_ITERATOR_DEBUG_LEVEL=0). Keep compatibility when this project uses Debug.
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

# Generate version header
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/Version.h.in
    ${CMAKE_BINARY_DIR}/include/yuan/Basic/Version.gen.h
    @ONLY
)

# Third-party dependencies
add_subdirectory(third_party)

# Source subdirectories
add_subdirectory(src)

# Runtime library
add_subdirectory(runtime)

# Inject runtime archive path into Driver link step.
if(TARGET yuan_driver AND TARGET yuan_runtime)
    target_compile_definitions(yuan_driver PRIVATE
        YUAN_RUNTIME_LIB_PATH=\"$<TARGET_FILE:yuan_runtime>\"
        YUAN_RUNTIME_LINK_FLAGS=\"${YUAN_RUNTIME_LINK_FLAGS}\"
    )
endif()

# Tools
add_subdirectory(tools)

# Tests
if(YUAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
    add_subdirectory(tests)
endif()

# Documentation
if(YUAN_BUILD_DOCS)
    add_subdirectory(docs)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "Yuan Compiler Configuration:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  LLVM Version: ${LLVM_VERSION}")
message(STATUS "  Build tests:  ${YUAN_BUILD_TESTS}")
message(STATUS "")
