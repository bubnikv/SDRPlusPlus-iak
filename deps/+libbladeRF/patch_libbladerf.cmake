#
# Run with -DSRC=<source-dir> -P this-script.
#
# libbladeRF v2.5.0's host/cmake/modules/FindLibUSB.cmake compiles+runs a tiny
# C program to read the libusb version. On a deps prefix not on PATH, the
# resulting executable can't load the libusb DLL during configure, the test
# fails, and it hardcodes LIBUSB_VERSION=0.0.0. libbladeRF's main CMakeLists
# then rejects this with "libusb v1.0.19 is required". We swap the fallback
# version to a known-OK value so the gate passes.
#
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/patch_helpers.cmake)

set(_f "${SRC}/host/cmake/modules/FindLibUSB.cmake")
file(READ "${_f}" _content)
# Catch any quoting / whitespace variation: set(LIBUSB_VERSION 0.0.0),
# set(LIBUSB_VERSION "0.0.0"), set ( LIBUSB_VERSION 0.0.0 ), etc.
string(REGEX REPLACE
    "set[ \t]*\\([ \t]*LIBUSB_VERSION[ \t]+\"?0\\.0\\.0\"?[ \t]*\\)"
    "set(LIBUSB_VERSION 1.0.27)  # patched by SDRIAK deps build"
    _content "${_content}")
file(WRITE "${_f}" "${_content}")
message(STATUS "Patched ${_f}")

# generate.c uses M_PI but doesn't define _USE_MATH_DEFINES. MSVC's <math.h>
# only exposes M_PI when that macro is defined before the first include.
set(_g "${SRC}/host/utilities/bladeRF-cli/src/cmd/generate.c")
file(READ "${_g}" _content)
if(NOT _content MATCHES "_USE_MATH_DEFINES")
    file(WRITE "${_g}" "#define _USE_MATH_DEFINES  /* patched by SDRIAK deps build */\n${_content}")
    message(STATUS "Patched ${_g}")
endif()

# FindLibUSB.cmake sets LIBUSB_LIBRARY_PATH_SUFFIX to 'MS64/dll' for MSVC x64,
# matching the old Windows binary distribution layout. Our destdir puts DLLs
# in 'bin/', so patch the suffix for MSVC so the DLL-copy custom command in
# host/CMakeLists.txt finds libusb-1.0.dll at the right location.
set(_u "${SRC}/host/cmake/modules/FindLibUSB.cmake")
file(READ "${_u}" _content)
patch_replace_or_fail(_content
    "set(LIBUSB_LIBRARY_PATH_SUFFIX MS64/dll)"
    "set(LIBUSB_LIBRARY_PATH_SUFFIX bin)  # patched by SDRIAK deps build")
file(WRITE "${_u}" "${_content}")
message(STATUS "Patched ${_u}")
