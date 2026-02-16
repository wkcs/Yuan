# Driver 模块

add_library(yuan_driver OBJECT
    Options.cpp
    Driver.cpp
)

target_include_directories(yuan_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(yuan_driver PRIVATE
    ${LLVM_LIBRARIES}
)

target_compile_definitions(yuan_driver PRIVATE
    ${LLVM_DEFINITIONS}
)