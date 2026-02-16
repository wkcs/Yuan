add_executable(yuanfilt
    main.cpp
)

target_include_directories(yuanfilt PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

set_target_properties(yuanfilt PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS yuanfilt DESTINATION bin)
