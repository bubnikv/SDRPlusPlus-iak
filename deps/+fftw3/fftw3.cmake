#
# FFTW3 — single-precision float build (matches Android kit / SDRIAK usage).
# Pure-CMake upstream, simplest possible recipe.
#
# Hash sourced from https://www.fftw.org/release-notes.html — verify on first
# successful build and pin if it diverges.
#
add_cmake_project(fftw3
    URL https://www.fftw.org/fftw-3.3.10.tar.gz
    URL_HASH SHA256=56c932549852cddcfafdab3820b0200c7742675be92179e59e6215b340e26467
    CMAKE_ARGS
        -DENABLE_FLOAT=ON
        -DBUILD_TESTS=OFF
        -DDISABLE_FORTRAN=ON
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
)

# Upstream installs Config.cmake under lib/cmake/fftw3f/ (single-precision
# package name) — recipe-name-keyed REQUIRES_CONFIG would mis-look-up, and
# the codebase consumes FFTW via FFTW_LIBRARIES pre-feed (see deps/CMakeLists.txt),
# not find_package(fftw3). File-existence probes are sufficient here.
sdrpp_validate_dep(fftw3
    TARGET    FFTW3::fftw3f
    LIB_NAMES fftw3f
    DLL_NAMES fftw3f.dll
    HEADER    fftw3.h)
