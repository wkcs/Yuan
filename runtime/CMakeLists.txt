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

target_link_libraries(yuan_runtime PUBLIC
    yuan_third_party_curl
    ${CMAKE_DL_LIBS}
)

# 安装运行时库
install(TARGETS yuan_runtime
    ARCHIVE DESTINATION lib
)

add_subdirectory(gui)
