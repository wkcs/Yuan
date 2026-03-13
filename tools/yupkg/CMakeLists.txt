add_executable(yupkg
    main.cpp
)

target_include_directories(yupkg PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(yupkg PRIVATE
    yuan_lib
)

set_target_properties(yupkg PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS yupkg DESTINATION bin)
