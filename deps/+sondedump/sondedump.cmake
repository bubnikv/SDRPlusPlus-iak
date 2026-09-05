#
# sondedump — dbdexter-dev's radiosonde decoder library.
#
# Upstream ships a CMake project that builds a static `radiosonde` library
# (the decoder core) plus a `sondedump` reference executable. SDRIAK only
# consumes the static library; the patch in this folder strips the executable
# build and adds install rules for the library and its public headers so the
# deps prefix exposes a normal find_package() surface.
#
# Headers land under <prefix>/include/sondedump/, consumed by the
# radiosonde_decoder module via `#include <sondedump/rs41.h>` etc.
#
add_cmake_project(sondedump
    GIT_REPOSITORY https://github.com/dbdexter-dev/sondedump
    GIT_TAG        528655564f34a42e464334fced4f7e89f9bfee46
    PATCH_COMMAND  ${CMAKE_COMMAND}
                       -DSRC=<SOURCE_DIR>
                       -P ${CMAKE_CURRENT_LIST_DIR}/patch_sondedump.cmake
    CMAKE_ARGS
        -DENABLE_TUI=OFF
        -DENABLE_AUDIO=OFF
        -DUNINSTALL_TARGET=OFF
        -DFULL_OPTIMIZE=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
)

set(_sondedump_link_libraries "")
if (NOT WIN32)
    list(APPEND _sondedump_link_libraries m)
endif ()

sdrpp_emit_imported_config(sondedump
    LIB_NAMES      radiosonde
    HEADER         sondedump/rs41.h
    LINK_LIBRARIES ${_sondedump_link_libraries}
)
