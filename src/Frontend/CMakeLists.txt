add_library(yuan_frontend OBJECT
    CompilerInvocation.cpp
    CompilerInstance.cpp
    FrontendAction.cpp
)

target_include_directories(yuan_frontend PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(yuan_frontend PRIVATE
    ${LLVM_LIBRARIES}
)

target_compile_definitions(yuan_frontend PRIVATE
    ${LLVM_DEFINITIONS}
)
