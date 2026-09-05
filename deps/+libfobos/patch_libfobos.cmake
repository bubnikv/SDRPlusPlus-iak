#
# Run with -DSRC=<source-dir> -P this-script.
#
# Patches libfobos's CMakeLists.txt:
#   1. Removes the hardcoded CMAKE_INSTALL_PREFIX so our destdir prefix is used.
#   2. Removes the bundled libusb-1.0.dll install rule (libusb is built
#      separately by dep_libusb).
#   3. Drops the explicit SHARED keyword from add_library(fobos ...) so the
#      library type follows BUILD_SHARED_LIBS. Without this, libfobos always
#      builds shared even when the dep policy resolves to static (the
#      classification in deps/cmake/DepClassification.cmake marks libfobos
#      static on every profile), producing fobos.dll + an import lib named
#      fobos.lib that the validator correctly flags as a static/shared
#      mismatch.
#   4. Applies the generic PkgConfig stub (idempotent).
#

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/patch_helpers.cmake)

set(_f "${SRC}/CMakeLists.txt")
if (NOT EXISTS "${_f}")
    message(FATAL_ERROR "patch_libfobos: ${_f} not found")
endif ()

file(READ "${_f}" _content)

# --- Patch 1: remove hardcoded install prefix ---
# Upstream sets: set(CMAKE_INSTALL_PREFIX "C:/Program Files/RigExpert/Fobos/")
# which would override the deps destdir.  Drop it.
patch_replace_or_fail(_content
    "set(CMAKE_INSTALL_PREFIX \"C:/Program Files/RigExpert/Fobos/\")"
    "# CMAKE_INSTALL_PREFIX hardcode removed by SDRIAK deps build")


# --- Patch 2: remove bundled libusb-1.0.dll install rule ---
# libfobos ships install(FILES $<TARGET_FILE_DIR:fobos>/libusb-1.0.dll ...)
# which tries to copy a DLL from the build dir that doesn't exist because we
# link against our own libusb from the destdir.  libusb is already installed
# by dep_libusb, so just drop this rule.
string(REGEX REPLACE
    "install[ \t]*\\([ \t]*FILES[ \t]+\\$<TARGET_FILE_DIR:fobos>/libusb-1\\.0\\.dll[^\n]*\n?"
    "# libusb-1.0.dll install removed by SDRIAK deps build (libusb built separately)\n"
    _content "${_content}")


# --- Patch 3: make library type follow BUILD_SHARED_LIBS ---
# Upstream hardcodes `add_library(fobos SHARED ${SRC})`, so BUILD_SHARED_LIBS=OFF
# is ignored and a DLL is always produced. Dropping the SHARED keyword lets
# CMake honor the flag we pass from the recipe.
patch_replace_or_fail(_content
    "add_library(fobos SHARED \${SRC})"
    "add_library(fobos \${SRC})")

file(WRITE "${_f}" "${_content}")

# --- Patch 3: PkgConfig stub (shared, idempotent) ---
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/patch_pkgconfig.cmake)
