# Yuan Unit Tests

include(GoogleTest)

# Add test subdirectories
add_subdirectory(Basic)
add_subdirectory(Lexer)
add_subdirectory(AST)
add_subdirectory(Builtin)
add_subdirectory(Parser)
add_subdirectory(Sema)
add_subdirectory(CodeGen)
add_subdirectory(Runtime)
