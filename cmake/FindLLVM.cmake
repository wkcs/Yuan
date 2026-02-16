# Find LLVM package
#
# This module finds LLVM and sets up the necessary variables and targets.
#
# Variables set:
#   LLVM_FOUND        - True if LLVM was found
#   LLVM_VERSION      - LLVM version string
#   LLVM_INCLUDE_DIRS - Include directories for LLVM
#   LLVM_LIBRARY_DIRS - Library directories for LLVM
#   LLVM_DEFINITIONS  - Compiler definitions for LLVM

# Try to find LLVM using llvm-config first
find_program(LLVM_CONFIG_EXECUTABLE
    NAMES llvm-config-20 llvm-config
    DOC "Path to llvm-config executable"
)

if(LLVM_CONFIG_EXECUTABLE)
    message(STATUS "Found llvm-config: ${LLVM_CONFIG_EXECUTABLE}")

    # Get LLVM version
    execute_process(
        COMMAND ${LLVM_CONFIG_EXECUTABLE} --version
        OUTPUT_VARIABLE LLVM_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Get LLVM include directory
    execute_process(
        COMMAND ${LLVM_CONFIG_EXECUTABLE} --includedir
        OUTPUT_VARIABLE LLVM_INCLUDE_DIRS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Get LLVM library directory
    execute_process(
        COMMAND ${LLVM_CONFIG_EXECUTABLE} --libdir
        OUTPUT_VARIABLE LLVM_LIBRARY_DIRS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Get LLVM definitions
    execute_process(
        COMMAND ${LLVM_CONFIG_EXECUTABLE} --cppflags
        OUTPUT_VARIABLE LLVM_CPPFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Get LLVM libraries
    execute_process(
        COMMAND ${LLVM_CONFIG_EXECUTABLE} --libs
            core support
            irreader bitwriter bitreader
            mc mcparser target targetparser
            analysis transformutils scalaropts instcombine passes
            all-targets
        OUTPUT_VARIABLE LLVM_LIBRARIES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Get system libraries
    execute_process(
        COMMAND ${LLVM_CONFIG_EXECUTABLE} --system-libs
        OUTPUT_VARIABLE LLVM_SYSTEM_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Get LLVM link flags
    execute_process(
        COMMAND ${LLVM_CONFIG_EXECUTABLE} --ldflags
        OUTPUT_VARIABLE LLVM_LDFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # llvm-config returns whitespace-delimited strings. Convert them to
    # proper CMake lists so each library/flag is passed as a separate item.
    if(WIN32)
        separate_arguments(LLVM_LIBRARIES WINDOWS_COMMAND "${LLVM_LIBRARIES}")
        separate_arguments(LLVM_SYSTEM_LIBS WINDOWS_COMMAND "${LLVM_SYSTEM_LIBS}")
        separate_arguments(LLVM_LDFLAGS WINDOWS_COMMAND "${LLVM_LDFLAGS}")
    else()
        separate_arguments(LLVM_LIBRARIES UNIX_COMMAND "${LLVM_LIBRARIES}")
        separate_arguments(LLVM_SYSTEM_LIBS UNIX_COMMAND "${LLVM_SYSTEM_LIBS}")
        separate_arguments(LLVM_LDFLAGS UNIX_COMMAND "${LLVM_LDFLAGS}")
    endif()
    if(WIN32)
        foreach(_llvm_sys_lib IN LISTS LLVM_SYSTEM_LIBS)
            if(_llvm_sys_lib MATCHES "^[Ll][Ii][Bb][Xx][Mm][Ll]2[Ss]\\.lib$")
                continue()
            endif()
            list(APPEND LLVM_LIBRARIES "${_llvm_sys_lib}")
        endforeach()
    else()
        list(APPEND LLVM_LIBRARIES ${LLVM_SYSTEM_LIBS})
    endif()

    set(LLVM_FOUND TRUE)

else()
    # Fall back to CMake's find_package
    find_package(LLVM CONFIG)

    if(LLVM_FOUND)
        message(STATUS "Found LLVM via CMake: ${LLVM_DIR}")

        # Map LLVM targets to libraries
        llvm_map_components_to_libnames(LLVM_LIBRARIES
            Core
            Support
            IRReader
            BitWriter
            BitReader
            MC
            MCParser
            Target
            Analysis
            TransformUtils
            ScalarOpts
            InstCombine
            Passes
            X86CodeGen
            X86AsmParser
            X86Desc
            X86Info
            AArch64CodeGen
            AArch64AsmParser
            AArch64Desc
            AArch64Info
        )
    endif()
endif()

if(LLVM_FOUND)
    message(STATUS "LLVM version: ${LLVM_VERSION}")
    message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")
    message(STATUS "LLVM library dirs: ${LLVM_LIBRARY_DIRS}")

    # Add LLVM include directories
    include_directories(${LLVM_INCLUDE_DIRS})
    link_directories(${LLVM_LIBRARY_DIRS})

    # Add LLVM definitions
    add_definitions(${LLVM_DEFINITIONS})

    # Store LLVM version for version header
    set(YUAN_LLVM_VERSION "${LLVM_VERSION}" CACHE STRING "LLVM version" FORCE)
else()
    message(WARNING "LLVM not found. Some features may not be available.")
    set(YUAN_LLVM_VERSION "not found" CACHE STRING "LLVM version" FORCE)
endif()
