# Yuan 编译器可执行文件

add_executable(yuanc
    main.cpp
)

target_include_directories(yuanc PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(yuanc PRIVATE
    yuan_lib
    ${LLVM_LIBRARIES}
)

target_compile_definitions(yuanc PRIVATE
    ${LLVM_DEFINITIONS}
)

if(TARGET libcurl_static)
    add_dependencies(yuanc libcurl_static)
elseif(TARGET libcurl)
    add_dependencies(yuanc libcurl)
endif()

if(TARGET yuan_runtime)
    add_dependencies(yuanc yuan_runtime)
endif()
if(TARGET yuan_runtime_net)
    add_dependencies(yuanc yuan_runtime_net)
endif()
if(TARGET yuan_gui_macos)
    add_dependencies(yuanc yuan_gui_macos)
elseif(TARGET yuan_gui_linux)
    add_dependencies(yuanc yuan_gui_linux)
elseif(TARGET yuan_gui_windows)
    add_dependencies(yuanc yuan_gui_windows)
endif()

# 设置 C++17 标准
set_target_properties(yuanc PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS yuanc DESTINATION bin)
