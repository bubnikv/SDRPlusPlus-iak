cmake_minimum_required(VERSION 3.13)
project(ft8_decoder)

# ---------------------------------------------------------------------------
# Sources
#
# All of src/ is built: the module itself (C++), the two engine wrappers
# (C++), and the vendored decoders (C):
#   - ft8_lib  (kgoba)        : FT8 / FT4
#   - kissfft                 : FFT used by ft8_lib
#   - wsprd core (K9AN/K1JT)  : WSPR
#
# metric_tables.c is #included directly into wspr_decode.c, so it must NOT be
# compiled as its own translation unit (that would duplicate symbols).
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")
list(FILTER SRC EXCLUDE REGEX "vendor/wspr/metric_tables\\.c$")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ---------------------------------------------------------------------------
# SDR++ module boilerplate (defines the shared-library target, links the
# SDR++ core, sets the output name, install rules, etc).
# ---------------------------------------------------------------------------
if (NOT SDRPP_MODULE_CMAKE)
    message(FATAL_ERROR
        "SDRPP_MODULE_CMAKE is not set. Build this module from inside the "
        "SDR++ source tree (place it in decoder_modules/ and enable "
        "OPT_BUILD_FT8_DECODER), not standalone.")
endif()
include(${SDRPP_MODULE_CMAKE})

# Vendored decoder headers. Everything under src/vendor is referenced with
# angle-bracket includes rooted at src/vendor (e.g. <ft8/decode.h>,
# <common/monitor.h>, <fft/kiss_fftr.h>); the engine headers live in src/.
target_include_directories(ft8_decoder PRIVATE "src/" "src/vendor/")

# The WSPR core uses single-precision FFTW. SDR++ already depends on FFTW, so
# this adds no new system dependency.
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3F REQUIRED fftw3f)
target_include_directories(ft8_decoder PRIVATE ${FFTW3F_INCLUDE_DIRS})
target_link_directories(ft8_decoder PRIVATE ${FFTW3F_LIBRARY_DIRS})
target_link_libraries(ft8_decoder PRIVATE ${FFTW3F_LIBRARIES})

# Some vendored C uses POSIX-2008 functions (e.g. stpcpy).
target_compile_definitions(ft8_decoder PRIVATE _GNU_SOURCE)
