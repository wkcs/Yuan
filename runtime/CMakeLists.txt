# Yuan Runtime Library

add_library(yuan_runtime STATIC
    yuan_format_typed.cpp
    yuan_varargs.cpp
    yuan_async.cpp
    yuan_os.cpp
    yuan_ffi.cpp
)

target_include_directories(yuan_runtime PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(yuan_runtime PRIVATE YUAN_ENABLE_NETWORK=0)

add_library(yuan_runtime_net STATIC
    yuan_net.cpp
)

target_include_directories(yuan_runtime_net PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

add_library(yuan_runtime_net_stub STATIC
    yuan_net.cpp
)

target_include_directories(yuan_runtime_net_stub PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_definitions(yuan_runtime_net_stub PRIVATE YUAN_ENABLE_NETWORK=0)

if(YUAN_ENABLE_NETWORK)
    target_compile_definitions(yuan_runtime_net PRIVATE YUAN_ENABLE_NETWORK=1)
    target_link_libraries(yuan_runtime_net PUBLIC yuan_third_party_curl)
else()
    target_compile_definitions(yuan_runtime_net PRIVATE YUAN_ENABLE_NETWORK=0)
endif()

set(_YUAN_RUNTIME_CORE_LINK_FLAG_ITEMS "")
if(CMAKE_DL_LIBS)
    foreach(_dl_lib IN LISTS CMAKE_DL_LIBS)
        if(_dl_lib STREQUAL "")
            continue()
        endif()
        if(_dl_lib MATCHES "^-l" OR _dl_lib MATCHES "^/" OR _dl_lib MATCHES "^-Wl,")
            list(APPEND _YUAN_RUNTIME_CORE_LINK_FLAG_ITEMS "${_dl_lib}")
        else()
            list(APPEND _YUAN_RUNTIME_CORE_LINK_FLAG_ITEMS "-l${_dl_lib}")
        endif()
    endforeach()
endif()
string(JOIN "|" _YUAN_RUNTIME_CORE_LINK_FLAGS ${_YUAN_RUNTIME_CORE_LINK_FLAG_ITEMS})
set(YUAN_RUNTIME_CORE_LINK_FLAGS "${_YUAN_RUNTIME_CORE_LINK_FLAGS}" CACHE INTERNAL "Runtime CORE link flags" FORCE)

target_link_libraries(yuan_runtime PUBLIC
    ${CMAKE_DL_LIBS}
)

target_link_libraries(yuan_runtime_net PUBLIC
    ${CMAKE_DL_LIBS}
)

# 安装运行时库
install(TARGETS yuan_runtime
    ARCHIVE DESTINATION lib
)

install(TARGETS yuan_runtime_net
    ARCHIVE DESTINATION lib
)

install(TARGETS yuan_runtime_net_stub
    ARCHIVE DESTINATION lib
)

add_subdirectory(gui)

set(_YUAN_GUI_RUNTIME_TARGET "")
if(TARGET yuan_gui_macos)
    set(_YUAN_GUI_RUNTIME_TARGET yuan_gui_macos)
elseif(TARGET yuan_gui_linux)
    set(_YUAN_GUI_RUNTIME_TARGET yuan_gui_linux)
elseif(TARGET yuan_gui_windows)
    set(_YUAN_GUI_RUNTIME_TARGET yuan_gui_windows)
endif()

if(NOT _YUAN_GUI_RUNTIME_TARGET STREQUAL "")
    set(YUAN_RUNTIME_GUI_LIB_PATH "$<TARGET_FILE:${_YUAN_GUI_RUNTIME_TARGET}>" CACHE INTERNAL "GUI runtime shared library path" FORCE)
else()
    set(YUAN_RUNTIME_GUI_LIB_PATH "" CACHE INTERNAL "GUI runtime shared library path" FORCE)
endif()
