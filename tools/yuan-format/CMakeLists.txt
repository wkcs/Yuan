add_executable(yuan-format
    main.cpp
)

target_include_directories(yuan-format PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(yuan-format PRIVATE
    yuan_lib
)

set_target_properties(yuan-format PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS yuan-format DESTINATION bin)
