# Yuan Compiler Version Configuration

set(YUAN_VERSION_MAJOR 0)
set(YUAN_VERSION_MINOR 1)
set(YUAN_VERSION_PATCH 0)
set(YUAN_VERSION "${YUAN_VERSION_MAJOR}.${YUAN_VERSION_MINOR}.${YUAN_VERSION_PATCH}")

# Get Git information
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE YUAN_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE YUAN_GIT_HASH_FULL
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE YUAN_GIT_DIRTY
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(YUAN_GIT_DIRTY)
        set(YUAN_GIT_HASH "${YUAN_GIT_HASH}-dirty")
    endif()
else()
    set(YUAN_GIT_HASH "unknown")
    set(YUAN_GIT_HASH_FULL "unknown")
endif()

# Build timestamp
string(TIMESTAMP YUAN_BUILD_TIME "%Y-%m-%d %H:%M:%S UTC" UTC)
