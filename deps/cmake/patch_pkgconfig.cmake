#
# Run with -DSRC=<source-dir> -P this-script.
#
# Generic patch for SDR libs whose CMakeLists.txt does
#   find_package(PkgConfig REQUIRED)
#   pkg_check_modules(<X> REQUIRED libusb-1.0)
#
# On Windows where pkg-config isn't on PATH, those calls fail. We:
#   1. Downgrade PkgConfig REQUIRED to QUIET so the configure continues.
#   2. Strip REQUIRED from any subsequent pkg_check_modules so they degrade
#      gracefully when PkgConfig wasn't found.
#   3. After the find_package(PkgConfig ...) call, inject a stub
#      `PkgConfig::libusb` IMPORTED INTERFACE target, since some libs
#      (libfobos, librfnm, ...) reference that target directly in their
#      target_link_libraries calls. It points at the LIBUSB_LIBRARY /
#      LIBUSB_INCLUDE_DIR cache vars that DEP_CMAKE_OPTS injects globally.
#
set(_f "${SRC}/CMakeLists.txt")
if (NOT EXISTS "${_f}")
    message(FATAL_ERROR "patch_pkgconfig: ${_f} not found")
endif ()

file(READ "${_f}" _content)

# Idempotency: skip if we already patched this file
if (_content MATCHES "SDR\\+\\+ deps build: stub PkgConfig")
    message(STATUS "patch_pkgconfig: ${_f} already patched, skipping")
    return()
endif ()

string(REGEX REPLACE
    "find_package[ \t]*\\([ \t]*PkgConfig[ \t]+REQUIRED[ \t]*\\)"
    "find_package(PkgConfig QUIET)"
    _content "${_content}")

string(REGEX REPLACE
    "pkg_check_modules[ \t]*\\([ \t]*([A-Za-z_][A-Za-z_0-9]*)[ \t]+REQUIRED[ \t]+([^\n\\)]+)\\)"
    "pkg_check_modules(\\1 \\2)"
    _content "${_content}")

set(_stub "
# >>> SDRIAK deps build: stub PkgConfig::libusb when pkg-config isn't available.
# Only fires when our deps tree has populated the override vars (bundled libusb
# path). When libusb is system-provided, LIBUSB_INCLUDE_DIR / LIBUSB_LIBRARY are
# empty — skip the stub so the subsequent pkg_check_modules() can populate the
# target from the host's pkg-config with the correct system paths.
if (NOT TARGET PkgConfig::libusb AND LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARY)
    add_library(PkgConfig::libusb INTERFACE IMPORTED)
    set_target_properties(PkgConfig::libusb PROPERTIES
        INTERFACE_LINK_LIBRARIES \"\${LIBUSB_LIBRARY}\"
        INTERFACE_INCLUDE_DIRECTORIES \"\${LIBUSB_INCLUDE_DIR}\")
endif ()
# <<< SDRIAK deps build")

string(REGEX REPLACE
    "(find_package[ \t]*\\([ \t]*PkgConfig[ \t]*[^\\)]*\\))"
    "\\1${_stub}"
    _content "${_content}")

file(WRITE "${_f}" "${_content}")
message(STATUS "patch_pkgconfig: patched ${_f}")
