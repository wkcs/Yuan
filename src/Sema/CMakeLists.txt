# Yuan Sema Module

add_library(yuan_sema OBJECT
    Sema.cpp
    Type.cpp
    Symbol.cpp
    Scope.cpp
    TypeChecker.cpp
    ModuleManager.cpp
    TypeCodec.cpp
)

target_include_directories(yuan_sema PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)
