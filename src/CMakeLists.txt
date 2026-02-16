# Yuan Compiler Source

# Collect all source files
add_subdirectory(Basic)
add_subdirectory(Lexer)
add_subdirectory(AST)
add_subdirectory(Parser)
add_subdirectory(Sema)
add_subdirectory(Builtin)
add_subdirectory(CodeGen)
add_subdirectory(Driver)

# Create the main Yuan library
add_library(yuan_lib STATIC
    $<TARGET_OBJECTS:yuan_basic>
    $<TARGET_OBJECTS:yuan_lexer>
    $<TARGET_OBJECTS:yuan_ast>
    $<TARGET_OBJECTS:yuan_parser>
    $<TARGET_OBJECTS:yuan_sema>
    $<TARGET_OBJECTS:yuan_builtin>
    $<TARGET_OBJECTS:yuan_codegen>
    $<TARGET_OBJECTS:yuan_driver>
)

target_include_directories(yuan_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

if(LLVM_FOUND)
    target_link_libraries(yuan_lib PUBLIC ${LLVM_LIBRARIES})
endif()

set_target_properties(yuan_lib PROPERTIES
    OUTPUT_NAME yuan
)
