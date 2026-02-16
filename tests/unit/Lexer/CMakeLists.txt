# Yuan Lexer Unit Tests

add_executable(lexer_tests
    TokenTest.cpp
    LiteralParserTest.cpp
    LexerCommentPropertyTest.cpp
    LexerIdentifierPropertyTest.cpp
    LexerNumberPropertyTest.cpp
    LexerStringPropertyTest.cpp
    LexerErrorPropertyTest.cpp
)

target_link_libraries(lexer_tests
    yuan_lib
    gtest
    gtest_main
)

target_include_directories(lexer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

gtest_discover_tests(lexer_tests)