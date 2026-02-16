# Basic Module Unit Tests

add_executable(basic_tests
    VersionTest.cpp
    SourceLocationTest.cpp
    SourceLocationPropertyTest.cpp
    SourceManagerTest.cpp
    DiagnosticTest.cpp
    DiagnosticPropertyTest.cpp
)

target_link_libraries(basic_tests PRIVATE
    yuan_lib
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(basic_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

gtest_discover_tests(basic_tests)
