add_library(yuan_tooling OBJECT
    ProjectConfig.cpp
    CompilerSession.cpp
    ToolRunner.cpp
)

target_include_directories(yuan_tooling PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(yuan_tooling PRIVATE
    yuan_third_party_json
    ${LLVM_LIBRARIES}
)

target_compile_definitions(yuan_tooling PRIVATE
    ${LLVM_DEFINITIONS}
)
