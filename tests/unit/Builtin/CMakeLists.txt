# Builtin Module Unit Tests

add_executable(builtin_tests
    BuiltinRegistryTest.cpp
)

target_link_libraries(builtin_tests PRIVATE
    yuan_lib
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(builtin_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

gtest_discover_tests(builtin_tests)
