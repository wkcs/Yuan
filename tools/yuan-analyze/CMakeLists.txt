add_executable(yuan-analyze
    main.cpp
)

target_include_directories(yuan-analyze PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(yuan-analyze PRIVATE
    yuan_lib
)

set_target_properties(yuan-analyze PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS yuan-analyze DESTINATION bin)
