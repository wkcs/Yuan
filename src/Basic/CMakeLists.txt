# Yuan Basic Module

add_library(yuan_basic OBJECT
    Version.cpp
    SourceLocation.cpp
    SourceManager.cpp
    Diagnostic.cpp
    DiagnosticIDs.cpp
    TextDiagnosticPrinter.cpp
    TokenKinds.cpp
)

target_include_directories(yuan_basic PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)
