# Yuan AST Module

add_library(yuan_ast OBJECT
    AST.cpp
    ASTContext.cpp
    Decl.cpp
    Expr.cpp
    Stmt.cpp
    Type.cpp
    Pattern.cpp
    ASTPrinter.cpp
    ASTDumper.cpp
)

target_include_directories(yuan_ast PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)
