# Yuan Sema Unit Tests

add_executable(sema_tests
    TypeTest.cpp
    TypeCheckerTest.cpp
    SymbolTableTest.cpp
    SymbolTablePropertyTest.cpp
    ModuleManagerTest.cpp
)

target_link_libraries(sema_tests
    yuan_sema
    yuan_builtin
    yuan_parser
    yuan_lexer
    yuan_ast
    yuan_basic
    yuan_codegen
    gtest
    gtest_main
)

if(LLVM_FOUND)
    target_include_directories(sema_tests PUBLIC ${LLVM_INCLUDE_DIRS})
    target_link_libraries(sema_tests ${LLVM_LIBRARIES})
endif()

target_include_directories(sema_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

add_test(NAME SemaTests COMMAND sema_tests)
