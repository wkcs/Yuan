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

# 设置 C++17 标准
set_target_properties(yuanc PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS yuanc DESTINATION bin)
