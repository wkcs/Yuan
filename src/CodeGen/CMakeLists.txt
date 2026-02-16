# Yuan CodeGen Module

add_library(yuan_codegen OBJECT
    CodeGen.cpp
    CGDecl.cpp
    CGExpr.cpp
    CGStmt.cpp
)

target_include_directories(yuan_codegen PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

if(LLVM_FOUND)
    target_include_directories(yuan_codegen PUBLIC ${LLVM_INCLUDE_DIRS})
endif()
