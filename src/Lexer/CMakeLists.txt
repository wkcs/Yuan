# Yuan Lexer Module

add_library(yuan_lexer OBJECT
    Token.cpp
    Lexer.cpp
    LiteralParser.cpp
)

target_include_directories(yuan_lexer PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(yuan_lexer PUBLIC
    yuan_basic
)