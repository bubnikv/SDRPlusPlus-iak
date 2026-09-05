#
# Run with -DSRC=<source-dir> -P this-script.
#
# libmirisdr-5 (ericek111 fork) builds two utility executables alongside the
# library: miri_sdr and miri_fm. miri_fm.c includes <pthread.h>, which isn't
# on the include path under MSVC (FindThreads only sets the link library, not
# the include path). SDRIAK only needs the library, so disable both utilities
# here rather than wiring up an include-path workaround.
#
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/patch_helpers.cmake)

set(_f "${SRC}/src/CMakeLists.txt")
if (NOT EXISTS "${_f}")
    message(FATAL_ERROR "patch_libmirisdr: ${_f} not found")
endif ()

file(READ "${_f}" _content)
# Normalize CRLF -> LF (git autocrlf may have converted on checkout) so the
# literal-string patches below match regardless of line-ending style.
string(REPLACE "\r\n" "\n" _content "${_content}")

# Drop the executable targets. Replacement strings deliberately don't echo
# the needle (which would re-match on a second run, breaking idempotency).
patch_replace_or_fail(_content
    "add_executable(miri_sdr miri_sdr.c)"
    "# miri_sdr executable disabled by SDRIAK deps build")
patch_replace_or_fail(_content
    "add_executable(miri_fm miri_fm.c)"
    "# miri_fm executable disabled by SDRIAK deps build")
patch_replace_or_fail(_content
    "set(INSTALL_TARGETS mirisdr_shared mirisdr_static miri_sdr miri_fm)"
    "set(INSTALL_TARGETS mirisdr_shared mirisdr_static)")

# Remove every target_link_libraries / set_property call that references the
# disabled targets — CMake errors out on missing target names. Multi-line
# calls are matched by [^)]* across lines (no ')' chars appear inside).
string(REGEX REPLACE
    "target_link_libraries\\([ \t]*miri_(sdr|fm)[^)]*\\)"
    "# target_link_libraries(miri_<x> ...) disabled by SDRIAK deps build"
    _content "${_content}")

string(REGEX REPLACE
    "set_property\\([ \t]*TARGET[ \t]+miri_(sdr|fm)[^)]*\\)"
    "# set_property(TARGET miri_<x> ...) disabled by SDRIAK deps build"
    _content "${_content}")

file(WRITE "${_f}" "${_content}")
message(STATUS "Patched ${_f}")
