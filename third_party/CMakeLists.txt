# Third-party dependencies used by Yuan runtime/toolchain.

option(YUAN_USE_BUNDLED_CURL "Use bundled curl from third_party/curl when present." ON)
set(_YUAN_BUNDLED_CURL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/curl")
set(_YUAN_USING_BUNDLED_CURL OFF)
set(_YUAN_USE_SYSTEM_CURL OFF)

function(yuan_configure_minimal_bundled_curl)
    # Keep bundled curl link surface minimal to avoid unresolved transitive
    # dependencies when Driver links via raw shell flags.
    set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
    set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
    set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)
endfunction()

if(YUAN_USE_BUNDLED_CURL AND EXISTS "${_YUAN_BUNDLED_CURL_DIR}/CMakeLists.txt")
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        # Build curl from source if OpenSSL is available.
        set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
        set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
        set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
        set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
        yuan_configure_minimal_bundled_curl()
        add_subdirectory(curl EXCLUDE_FROM_ALL)

        if(TARGET libcurl AND NOT TARGET CURL::libcurl)
            add_library(CURL::libcurl ALIAS libcurl)
        elseif(TARGET libcurl_static AND NOT TARGET CURL::libcurl)
            add_library(CURL::libcurl ALIAS libcurl_static)
        endif()
        set(_YUAN_USING_BUNDLED_CURL ON)
    else()
        # No OpenSSL: prefer system libcurl first to keep HTTPS support if possible.
        find_package(CURL QUIET)
        if(CURL_FOUND)
            set(_YUAN_USE_SYSTEM_CURL ON)
            message(STATUS "OpenSSL not found; using system libcurl (likely TLS-enabled).")
        else()
            # Last resort: bundled curl without TLS.
            set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
            set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
            set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
            set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
            set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
            set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
            set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
            yuan_configure_minimal_bundled_curl()
            if(WIN32)
                # OpenSSL unavailable on Windows: use native Schannel so HTTPS still works.
                set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
            endif()
            add_subdirectory(curl EXCLUDE_FROM_ALL)

            if(TARGET libcurl AND NOT TARGET CURL::libcurl)
                add_library(CURL::libcurl ALIAS libcurl)
            elseif(TARGET libcurl_static AND NOT TARGET CURL::libcurl)
                add_library(CURL::libcurl ALIAS libcurl_static)
            endif()
            set(_YUAN_USING_BUNDLED_CURL ON)
            if(WIN32)
                message(STATUS "OpenSSL and system libcurl unavailable; bundled curl will use Windows Schannel TLS backend.")
            else()
                message(STATUS "OpenSSL and system libcurl both unavailable; bundled curl will be built without TLS.")
            endif()
        endif()
    endif()
else()
    set(_YUAN_USE_SYSTEM_CURL ON)
endif()

if(_YUAN_USE_SYSTEM_CURL)
    find_package(CURL REQUIRED)
endif()

if(NOT TARGET CURL::libcurl)
    message(FATAL_ERROR "CURL::libcurl target not available. Provide system curl or vendor curl in third_party/curl.")
endif()

add_library(yuan_third_party_curl INTERFACE)
target_link_libraries(yuan_third_party_curl INTERFACE CURL::libcurl)

# Driver links final executables via a raw shell command, so keep a
# straightforward link flag for runtime transitive dependencies.
if(_YUAN_USING_BUNDLED_CURL)
    set(_YUAN_RUNTIME_LINK_FLAG_ITEMS "$<TARGET_LINKER_FILE:CURL::libcurl>")
    # The driver links with a raw shell command, so static libcurl transitive
    # deps (OpenSSL) must be added explicitly.
    if(OpenSSL_FOUND)
        if(OPENSSL_SSL_LIBRARY)
            list(APPEND _YUAN_RUNTIME_LINK_FLAG_ITEMS "${OPENSSL_SSL_LIBRARY}")
        endif()
        if(OPENSSL_CRYPTO_LIBRARY)
            list(APPEND _YUAN_RUNTIME_LINK_FLAG_ITEMS "${OPENSSL_CRYPTO_LIBRARY}")
        endif()
    endif()
    # Static libcurl may use zlib for content encoding; add it explicitly.
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        list(APPEND _YUAN_RUNTIME_LINK_FLAG_ITEMS "${ZLIB_LIBRARIES}")
    endif()
else()
    set(_YUAN_RUNTIME_LINK_FLAG_ITEMS "-lcurl")
endif()
if(CMAKE_DL_LIBS)
    foreach(_dl_lib IN LISTS CMAKE_DL_LIBS)
        if(_dl_lib STREQUAL "")
            continue()
        endif()
        if(_dl_lib MATCHES "^-l" OR _dl_lib MATCHES "^/" OR _dl_lib MATCHES "^-Wl,")
            list(APPEND _YUAN_RUNTIME_LINK_FLAG_ITEMS "${_dl_lib}")
        else()
            list(APPEND _YUAN_RUNTIME_LINK_FLAG_ITEMS "-l${_dl_lib}")
        endif()
    endforeach()
endif()
if(APPLE)
    list(APPEND _YUAN_RUNTIME_LINK_FLAG_ITEMS
        "-framework CoreFoundation"
        "-framework SystemConfiguration"
    )
endif()

if(WIN32)
    # When linking a static libcurl archive manually (driver shell link step),
    # include the common Win32 transitive system libraries explicitly.
    list(APPEND _YUAN_RUNTIME_LINK_FLAG_ITEMS
        "-lws2_32"
        "-lwldap32"
        "-liphlpapi"
        "-lbcrypt"
        "-lcrypt32"
        "-ladvapi32"
        "-lsecur32"
        "-lnormaliz"
    )
endif()

string(JOIN " " _YUAN_RUNTIME_LINK_FLAGS ${_YUAN_RUNTIME_LINK_FLAG_ITEMS})
set(YUAN_RUNTIME_LINK_FLAGS "${_YUAN_RUNTIME_LINK_FLAGS}" CACHE INTERNAL "Runtime link flags")

# nlohmann_json (Header-only)
add_library(yuan_third_party_json INTERFACE)
target_include_directories(yuan_third_party_json INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json/include>
)
