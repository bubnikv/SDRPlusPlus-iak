#
# Run with -DSRC=<source-dir> -P this-script.
#
# Patches librfnm's CMakeLists.txt:
#   1. Removes the hardcoded CMAKE_INSTALL_PREFIX so our destdir prefix is used.
#   2. Removes the bundled libusb-1.0.dll install rule (we build libusb ourselves).
#   3. Applies the generic PkgConfig stub (idempotent).
#

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/patch_helpers.cmake)

set(_f "${SRC}/CMakeLists.txt")
if (NOT EXISTS "${_f}")
    message(FATAL_ERROR "patch_librfnm: ${_f} not found")
endif ()

file(READ "${_f}" _content)

# --- Patch 1: remove hardcoded install prefix ---
patch_replace_or_fail(_content
    "set(CMAKE_INSTALL_PREFIX \"C:/Program Files/RFNM/\")"
    "# CMAKE_INSTALL_PREFIX hardcode removed by SDRIAK deps build")

# --- Patch 2: remove bundled DLL install rules ---
# librfnm ships install rules copying spdlog.dll, fmt.dll, and libusb-1.0.dll
# from $<TARGET_FILE_DIR:rfnm> — those DLLs don't exist there because we build
# each dependency separately. They are already installed by their own dep targets.
foreach(_dll "spdlog\\.dll" "fmt\\.dll" "libusb-1\\.0\\.dll")
    string(REGEX REPLACE
        "install[ \t]*\\([ \t]*FILES[ \t]+\\$<TARGET_FILE_DIR:rfnm>/${_dll}[^\n]*\n?"
        "# bundled DLL install removed by SDRIAK deps build\n"
        _content "${_content}")
endforeach()

file(WRITE "${_f}" "${_content}")

# --- Patch 3: PkgConfig stub (shared, idempotent) ---
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/patch_pkgconfig.cmake)
