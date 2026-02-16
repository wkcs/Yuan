# Yuan Parser Module

add_library(yuan_parser OBJECT
    Parser.cpp
    ParseDecl.cpp
    ParseExpr.cpp
    ParseStmt.cpp
    ParseType.cpp
    ParsePattern.cpp
)

target_include_directories(yuan_parser PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)
