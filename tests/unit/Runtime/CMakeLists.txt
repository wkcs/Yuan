# Yuan Runtime unit tests

add_executable(runtime_tests
    AsyncRuntimeTest.cpp
)

target_link_libraries(runtime_tests
    yuan_runtime
    GTest::gtest_main
)

target_include_directories(runtime_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(runtime_tests)
