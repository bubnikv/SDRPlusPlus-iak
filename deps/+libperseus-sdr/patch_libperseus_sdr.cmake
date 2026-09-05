#
# Run with -DSRC=<source-dir> -P this-script.
#
# Patches bubnikv/libperseus-sdr's CMakeLists.txt (inherited from Rouma):
#   1. Drops the explicit SHARED keyword from add_library(perseus-sdr ...) so
#      the library type follows BUILD_SHARED_LIBS. Without this, the dep
#      always builds shared even when the policy resolves to static (per
#      DepClassification.cmake, libperseus-sdr is static on every profile),
#      producing perseus-sdr.dll + an import lib named perseus-sdr.lib that
#      the validator correctly flags as a static/shared mismatch (same issue
#      libfobos had before its own patch landed).
#   2. Gates the pkg_check_modules(libusb-1.0) call on LIBUSB not already
#      being resolved by the caller. The parent autobuild passes
#      -DLIBUSB_INCLUDE_DIRS / -DLIBUSB_LIBRARIES / -DLIBUSB_FOUND when libusb
#      is built as a bundled dep (no system .pc file), but upstream's
#      unconditional pkg_check_modules ignores those and fails the configure.
#   3. Appends install rules — upstream ships none, so cmake --target install
#      would otherwise fail with "unknown target 'install'".
#
# C/H source patches (GCC 14 implicit-declaration, variadic-macro trailing
# comma) live as commits in bubnikv/libperseus-sdr now, not here. See that
# fork's README for details.
#

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/patch_helpers.cmake)

set(_f "${SRC}/CMakeLists.txt")
file(READ "${_f}" _content)

# --- Patch 1: make library type follow BUILD_SHARED_LIBS ---
patch_replace_or_fail(_content
    "add_library(perseus-sdr SHARED \${SRC})"
    "add_library(perseus-sdr \${SRC})")

# --- Patch 2: skip pkg_check_modules when LIBUSB is pre-resolved by parent ---
patch_replace_or_fail(_content
    "    find_package(PkgConfig)

    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)"
    "    if (NOT LIBUSB_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    endif ()")

# --- Patch 3: append install rules (upstream has none) ---
# Idempotency via marker — patch_replace_or_fail can't help here because this
# is an append, not a substitution.
if (NOT _content MATCHES "SDR\\+\\+ deps build: install rules")
    string(APPEND _content "
# >>> SDRIAK deps build: install rules (none existed upstream)
include(GNUInstallDirs)
install(TARGETS perseus-sdr
    RUNTIME DESTINATION \${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION \${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION \${CMAKE_INSTALL_LIBDIR}
)
install(FILES
    \${CMAKE_CURRENT_SOURCE_DIR}/src/perseus-sdr.h
    DESTINATION \${CMAKE_INSTALL_INCLUDEDIR}/perseus-sdr
)
# <<< SDRIAK deps build
")
endif ()

file(WRITE "${_f}" "${_content}")
