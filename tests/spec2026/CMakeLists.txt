find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_test(
    NAME spec2026_all
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/scripts/test_spec2026.py --profile all
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME spec2026_validate
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/scripts/spec2026_validate_manifest.py --min-cases-per-point 3
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME spec2026_report
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/scripts/spec2026_coverage_report.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(spec2026_all PROPERTIES TIMEOUT 1800)
set_tests_properties(spec2026_validate PROPERTIES TIMEOUT 300)
set_tests_properties(spec2026_report PROPERTIES TIMEOUT 300)
