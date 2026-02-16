# Yuan AST Unit Tests

add_executable(ast_tests
    ASTNodeTest.cpp
    ASTPrinterTest.cpp
)

target_link_libraries(ast_tests
    yuan_lib
    GTest::gtest_main
)

target_include_directories(ast_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

gtest_discover_tests(ast_tests)
