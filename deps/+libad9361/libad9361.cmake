#
# libad9361-iio completes PlutoSDR support. Upstream's `install` target is
# broken on most platforms (the Android kit notes this), so we keep the common
# CMake configure/build path but copy the artifacts ourselves.
#

set(_libad9361_iio_args "")
if (WIN32)
    list(APPEND _libad9361_iio_args
        -DLIBIIO_INCLUDEDIR=${SDRPP_DEPS_INSTALL_PREFIX}/include
        -DLIBIIO_LIBRARIES=${SDRPP_DEPS_INSTALL_PREFIX}/lib/libiio.lib
    )
endif ()

# Apple Clang 16+ (Xcode 15+) promotes -Wimplicit-function-declaration to a
# hard error by default. libad9361 v0.2's ad9361_baseband_auto_rate.c calls
# snprintf (whose declaration *does* live in the <stdio.h> the source already
# includes), but Clang errors anyway — likely an interaction with its
# library-function detection. Downgrade the diagnostic for this sub-build
# rather than carrying a fragile source patch that has to track the upstream
# header layout.
if (APPLE)
    list(APPEND _libad9361_iio_args
        -DCMAKE_C_FLAGS=-Wno-error=implicit-function-declaration
    )
endif ()

add_cmake_project(libad9361
    URL                 https://github.com/analogdevicesinc/libad9361-iio/archive/refs/tags/v0.2.tar.gz
    URL_HASH            SHA256=f0d935eb7f70fde8596d98e9eb1f311b408b87662fc1fff45ae3ed455697b747
    PATCH_COMMAND       ${CMAKE_COMMAND}
                            -DSRC=<SOURCE_DIR>
                            -P ${CMAKE_CURRENT_LIST_DIR}/patch_libad9361.cmake
    CMAKE_ARGS
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${_libad9361_iio_args}
    # Upstream's default ALL target builds test executables that do not link
    # cleanly against the bundled libiio import library on MSVC. SDRIAK only
    # needs the ad9361 library, so build that target and let our custom install
    # step copy the produced artifacts.
    BUILD_COMMAND
        ${CMAKE_COMMAND} --build . --target ad9361 --config ${CMAKE_BUILD_TYPE}
    INSTALL_COMMAND
            ${CMAKE_COMMAND} -E make_directory ${SDRPP_DEPS_INSTALL_PREFIX}/include
        COMMAND
            ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/ad9361.h ${SDRPP_DEPS_INSTALL_PREFIX}/include/ad9361.h
        COMMAND
            ${CMAKE_COMMAND} -E make_directory ${SDRPP_DEPS_INSTALL_PREFIX}/bin
        COMMAND
            ${CMAKE_COMMAND} -E make_directory ${SDRPP_DEPS_INSTALL_PREFIX}/lib
        # Copy whatever the build produced, handled at build time via a glob.
        COMMAND
            ${CMAKE_COMMAND} -DSRC=<BINARY_DIR> -DCONFIG=${CMAKE_BUILD_TYPE} -DDST_BIN=${SDRPP_DEPS_INSTALL_PREFIX}/bin -DDST_LIB=${SDRPP_DEPS_INSTALL_PREFIX}/lib
                             -P ${CMAKE_CURRENT_LIST_DIR}/install_libad9361.cmake
)

set(DEP_libad9361_DEPENDS libiio)

set(_libad9361_compile_definitions "")
set(_libad9361_package_dependencies "")
set(_libad9361_link_libraries "")
sdrpp_dep_builds_shared(libad9361 _libad9361_builds_shared)
if (NOT _libad9361_builds_shared)
    list(APPEND _libad9361_compile_definitions LIBAD9361_STATIC)
    list(APPEND _libad9361_package_dependencies libiio)
    list(APPEND _libad9361_link_libraries libiio::libiio)
endif ()

sdrpp_emit_imported_config(libad9361
    LIB_NAMES   ad9361 libad9361
    DLL_NAMES   ad9361.dll libad9361.dll
    HEADER      ad9361.h
    COMPILE_DEFINITIONS ${_libad9361_compile_definitions}
    PACKAGE_DEPENDENCIES ${_libad9361_package_dependencies}
    LINK_LIBRARIES ${_libad9361_link_libraries}
)
