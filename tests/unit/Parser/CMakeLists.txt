# Yuan Parser Unit Tests

add_executable(parser_tests
    ParseDeclTest.cpp
    ParseTypeTest.cpp
    ParseExprTest.cpp
    ParseExprOperatorPropertyTest.cpp
    ParseStmtTest.cpp
    ParsePatternTest.cpp
    ParseErrorRecoveryPropertyTest.cpp
    ParseRoundTripPropertyTest.cpp
)

target_link_libraries(parser_tests
    yuan_lib
    GTest::gtest_main
)

target_include_directories(parser_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

gtest_discover_tests(parser_tests)
