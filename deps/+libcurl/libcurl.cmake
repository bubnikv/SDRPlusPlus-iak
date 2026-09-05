#
# libcurl — HTTP/WebSocket transport linked privately into sdrpp_core.
#
# Reworked from cmake/find_or_fetch_curl.cmake. Static + PIC build pinned to
# curl-8_11_1 (≥ 8.11 lifts WebSockets out of experimental). Protocols are
# trimmed to the surface SDRIAK uses; TLS backend is platform-native:
#   - Windows: Schannel
#   - macOS:   Secure Transport
#   - Android: MbedTLS (built by deps/+mbedtls)
#   - Linux:   system OpenSSL (only matters when libcurl is forced bundled;
#              the distro profile resolves to system curl)
#
# No CA bundle is shipped. On Android the runtime must point libcurl at
# /system/etc/security/cacerts via CURLOPT_CAPATH (see core/utils/curl_init).
#
sdrpp_dep_get_linkage_option_bools(libcurl _libcurl_build_shared _libcurl_build_static)

set(_libcurl_cmake_args
    -DBUILD_CURL_EXE=OFF
    -DBUILD_TESTING=OFF
    -DBUILD_LIBCURL_DOCS=OFF
    -DBUILD_MISC_DOCS=OFF
    -DENABLE_CURL_MANUAL=OFF
    -DCURL_DISABLE_INSTALL=OFF
    -DCURL_ENABLE_EXPORT_TARGET=ON
    -DCURL_USE_LIBPSL=OFF
    -DUSE_APPLE_IDN=OFF
    -DUSE_LIBIDN2=OFF
    -DUSE_WIN32_IDN=OFF
    -DUSE_NGHTTP2=OFF
    -DCURL_USE_LIBSSH2=OFF
    -DENABLE_WEBSOCKETS=ON
    -DCURL_DISABLE_LDAP=ON
    -DCURL_DISABLE_LDAPS=ON
    -DCURL_DISABLE_FTP=ON
    -DCURL_DISABLE_DICT=ON
    -DCURL_DISABLE_TELNET=ON
    -DCURL_DISABLE_TFTP=ON
    -DCURL_DISABLE_RTSP=ON
    -DCURL_DISABLE_SMTP=ON
    -DCURL_DISABLE_POP3=ON
    -DCURL_DISABLE_IMAP=ON
    -DCURL_DISABLE_GOPHER=ON
    -DCURL_DISABLE_MQTT=ON
    -DCURL_DISABLE_SCP=ON
    -DCURL_DISABLE_SFTP=ON
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON)

if (WIN32)
    list(APPEND _libcurl_cmake_args -DCURL_USE_SCHANNEL=ON)
elseif (APPLE)
    list(APPEND _libcurl_cmake_args -DCURL_USE_SECTRANSP=ON)
elseif (ANDROID)
    # Pre-feed curl's bundled FindMbedTLS.cmake with the deps install layout
    # so it doesn't have to call find_package(MbedTLS) from a stale module path.
    set(_mbedtls_inc "${SDRPP_DEPS_INSTALL_PREFIX}/include")
    set(_mbedtls_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/libmbedtls.a")
    set(_mbedx509_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/libmbedx509.a")
    set(_mbedcrypto_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/libmbedcrypto.a")
    list(APPEND _libcurl_cmake_args
        -DCURL_USE_MBEDTLS=ON
        -DMBEDTLS_INCLUDE_DIRS=${_mbedtls_inc}
        -DMBEDTLS_LIBRARY=${_mbedtls_lib}
        -DMBEDX509_LIBRARY=${_mbedx509_lib}
        -DMBEDCRYPTO_LIBRARY=${_mbedcrypto_lib})
else ()
    list(APPEND _libcurl_cmake_args -DCURL_USE_OPENSSL=ON)
endif ()

add_cmake_project(libcurl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG        curl-8_11_1
    GIT_SHALLOW    ON
    CMAKE_ARGS     ${_libcurl_cmake_args})

if (ANDROID)
    set(DEP_libcurl_DEPENDS mbedtls)
endif ()

sdrpp_validate_dep(libcurl
    PACKAGE_NAME   CURL
    TARGET         CURL::libcurl
    LIB_NAMES      curl libcurl
    DLL_NAMES      libcurl.dll curl.dll
    HEADER         curl.h
    INCLUDE_SUBDIR curl
    REQUIRES_CONFIG)
