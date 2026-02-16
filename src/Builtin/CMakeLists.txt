# Yuan Builtin Module

add_library(yuan_builtin OBJECT
    BuiltinRegistry.cpp
    ImportBuiltin.cpp
    SizeofBuiltin.cpp
    AlignofBuiltin.cpp
    TypeofBuiltin.cpp
    PlatformBuiltin.cpp
    PanicBuiltin.cpp
    AssertBuiltin.cpp
    LocationBuiltin.cpp
    PrintBuiltin.cpp
    FormatBuiltin.cpp
    MemoryBuiltin.cpp
    AsyncBuiltin.cpp
    OSBuiltin.cpp
    FFIBuiltin.cpp
)

target_include_directories(yuan_builtin PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)
