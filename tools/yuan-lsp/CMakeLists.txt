add_executable(yuan-lsp
    main.cpp
    JSONRPC.cpp
    LSPServer.cpp
)

target_include_directories(yuan-lsp PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(yuan-lsp PRIVATE
    yuan_lib
    yuan_third_party_json
)
