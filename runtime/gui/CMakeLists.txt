set(YUAN_GUI_OUTPUT_DIR "${CMAKE_BINARY_DIR}/gui")

if(APPLE)
    enable_language(OBJCXX)

    add_library(yuan_gui_macos SHARED
        yuan_gui_macos.mm
    )

    target_compile_features(yuan_gui_macos PRIVATE cxx_std_17)
    target_link_libraries(yuan_gui_macos PRIVATE "-framework Cocoa")

    set_target_properties(yuan_gui_macos PROPERTIES
        OUTPUT_NAME "yuan_gui_macos"
        LIBRARY_OUTPUT_DIRECTORY "${YUAN_GUI_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${YUAN_GUI_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${YUAN_GUI_OUTPUT_DIR}"
    )

    install(TARGETS yuan_gui_macos LIBRARY DESTINATION lib)
endif()

if(UNIX AND NOT APPLE)
    find_package(X11 QUIET)
    add_library(yuan_gui_linux SHARED
        yuan_gui_linux.cpp
    )

    target_compile_features(yuan_gui_linux PRIVATE cxx_std_17)
    if(X11_FOUND)
        target_include_directories(yuan_gui_linux PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(yuan_gui_linux PRIVATE ${X11_LIBRARIES})
    else()
        message(WARNING "X11 not found: yuan_gui_linux will be built as a stub backend.")
    endif()

    set_target_properties(yuan_gui_linux PROPERTIES
        OUTPUT_NAME "yuan_gui_linux"
        LIBRARY_OUTPUT_DIRECTORY "${YUAN_GUI_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${YUAN_GUI_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${YUAN_GUI_OUTPUT_DIR}"
    )

    install(TARGETS yuan_gui_linux LIBRARY DESTINATION lib)
endif()

if(WIN32)
    add_library(yuan_gui_windows SHARED
        yuan_gui_windows.cpp
    )

    target_compile_features(yuan_gui_windows PRIVATE cxx_std_17)
    target_compile_definitions(yuan_gui_windows PRIVATE YUAN_GUI_EXPORTS)
    target_link_libraries(yuan_gui_windows PRIVATE user32 gdi32)

    set_target_properties(yuan_gui_windows PROPERTIES
        OUTPUT_NAME "yuan_gui_windows"
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        LIBRARY_OUTPUT_DIRECTORY "${YUAN_GUI_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${YUAN_GUI_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${YUAN_GUI_OUTPUT_DIR}"
    )

    install(TARGETS yuan_gui_windows
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
    )
endif()
