# CodeGen unit tests
set(YUAN_GTEST_DISCOVERY_TIMEOUT 60)

add_executable(TypeConversionTest
    TypeConversionTest.cpp
)

target_link_libraries(TypeConversionTest
    yuan_lib
    GTest::gtest_main
)

if(LLVM_FOUND)
    target_include_directories(TypeConversionTest PUBLIC ${LLVM_INCLUDE_DIRS})
    target_link_libraries(TypeConversionTest ${LLVM_LIBRARIES})
endif()

gtest_discover_tests(TypeConversionTest DISCOVERY_TIMEOUT ${YUAN_GTEST_DISCOVERY_TIMEOUT})

add_executable(DeclCodeGenTest
    DeclCodeGenTest.cpp
)

target_link_libraries(DeclCodeGenTest
    yuan_lib
    GTest::gtest_main
)

if(LLVM_FOUND)
    target_include_directories(DeclCodeGenTest PUBLIC ${LLVM_INCLUDE_DIRS})
    target_link_libraries(DeclCodeGenTest ${LLVM_LIBRARIES})
endif()

gtest_discover_tests(DeclCodeGenTest DISCOVERY_TIMEOUT ${YUAN_GTEST_DISCOVERY_TIMEOUT})

add_executable(StmtCodeGenTest
    StmtCodeGenTest.cpp
)

target_link_libraries(StmtCodeGenTest
    yuan_lib
    GTest::gtest_main
)

if(LLVM_FOUND)
    target_include_directories(StmtCodeGenTest PUBLIC ${LLVM_INCLUDE_DIRS})
    target_link_libraries(StmtCodeGenTest ${LLVM_LIBRARIES})
endif()

gtest_discover_tests(StmtCodeGenTest DISCOVERY_TIMEOUT ${YUAN_GTEST_DISCOVERY_TIMEOUT})

add_executable(ErrorHandlingTest
    ErrorHandlingTest.cpp
)

target_link_libraries(ErrorHandlingTest
    yuan_lib
    GTest::gtest_main
)

if(LLVM_FOUND)
    target_include_directories(ErrorHandlingTest PUBLIC ${LLVM_INCLUDE_DIRS})
    target_link_libraries(ErrorHandlingTest ${LLVM_LIBRARIES})
endif()

gtest_discover_tests(ErrorHandlingTest DISCOVERY_TIMEOUT ${YUAN_GTEST_DISCOVERY_TIMEOUT})

add_executable(IREmissionTest
    IREmissionTest.cpp
)

target_link_libraries(IREmissionTest
    yuan_lib
    GTest::gtest_main
)

if(LLVM_FOUND)
    target_include_directories(IREmissionTest PUBLIC ${LLVM_INCLUDE_DIRS})
    target_link_libraries(IREmissionTest ${LLVM_LIBRARIES})
endif()

gtest_discover_tests(IREmissionTest DISCOVERY_TIMEOUT ${YUAN_GTEST_DISCOVERY_TIMEOUT})

add_executable(CodeGenPropertyTest
    CodeGenPropertyTest.cpp
)

target_link_libraries(CodeGenPropertyTest
    yuan_lib
    GTest::gtest_main
)

if(LLVM_FOUND)
    target_include_directories(CodeGenPropertyTest PUBLIC ${LLVM_INCLUDE_DIRS})
    target_link_libraries(CodeGenPropertyTest ${LLVM_LIBRARIES})
endif()

gtest_discover_tests(CodeGenPropertyTest DISCOVERY_TIMEOUT ${YUAN_GTEST_DISCOVERY_TIMEOUT})

add_executable(TargetCodeGenTest
    TargetCodeGenTest.cpp
)

target_link_libraries(TargetCodeGenTest
    yuan_lib
    GTest::gtest_main
)

if(LLVM_FOUND)
    target_include_directories(TargetCodeGenTest PUBLIC ${LLVM_INCLUDE_DIRS})
    target_link_libraries(TargetCodeGenTest ${LLVM_LIBRARIES})
endif()

gtest_discover_tests(TargetCodeGenTest DISCOVERY_TIMEOUT ${YUAN_GTEST_DISCOVERY_TIMEOUT})

add_executable(GenericMonomorphTest
    GenericMonomorphTest.cpp
)

target_link_libraries(GenericMonomorphTest
    yuan_lib
    GTest::gtest_main
)

if(LLVM_FOUND)
    target_include_directories(GenericMonomorphTest PUBLIC ${LLVM_INCLUDE_DIRS})
    target_link_libraries(GenericMonomorphTest ${LLVM_LIBRARIES})
endif()

gtest_discover_tests(GenericMonomorphTest DISCOVERY_TIMEOUT ${YUAN_GTEST_DISCOVERY_TIMEOUT})
