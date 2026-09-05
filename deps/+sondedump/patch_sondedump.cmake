#
# Run with -DSRC=<source-dir> -P this-script.
#
# Patches dbdexter-dev/sondedump's CMakeLists.txt:
#   1. Replaces the broken ARM detection (`execute_process(... grep ...)`).
#      `grep` isn't on PATH under MSVC, and the test pattern matches arm64
#      Apple clang too — leading to an unrecognized -mfpu=auto. Probe the
#      flag instead of matching `-dumpmachine` output.
#   2. SOURCE_BASE_PATH_SIZE is computed from CMAKE_SOURCE_DIR, which is
#      the parent project when sondedump is built as a sub-project — the
#      resulting offset is bogus. Use CMAKE_CURRENT_SOURCE_DIR instead.
#   3. Drops the `sondedump` reference executable and its install rule —
#      SDRIAK only consumes the `radiosonde` static library.
#   4. Adds install rules for the radiosonde library and the public API
#      headers under include/sondedump/.
#   5. Fixes an RS41 serial-field over-read that Android's fortified libc
#      detects and aborts while decoding.
#
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/patch_helpers.cmake)

set(_f "${SRC}/CMakeLists.txt")
if (NOT EXISTS "${_f}")
    message(FATAL_ERROR "patch_sondedump: ${_f} not found")
endif ()

file(READ "${_f}" _content)
# Normalize CRLF -> LF so literal-string patches match regardless of how the
# source was extracted (git autocrlf, tarball, etc.).
string(REPLACE "\r\n" "\n" _content "${_content}")

# --- Patch 1: cross-toolchain-safe ARM tuning ---
patch_replace_or_fail(_content
"# ARM architectures need -mfpu=auto in order to enable NEON when available,
# but that option is unrecognized by x86 gcc (and possibly others): only
# add it to the release flags when the compiler's target is arm
# This is not a problem for arm64, as NEON support is mandatory for that arch
execute_process(COMMAND \"\${CMAKE_C_COMPILER}\" \"-dumpmachine\" COMMAND \"grep\" \"arm\" OUTPUT_QUIET RESULT_VARIABLE is_arm)
if (is_arm EQUAL \"0\")
\tset(CMAKE_C_FLAGS_RELEASE \"\${CMAKE_C_FLAGS_RELEASE} -mcpu=native -mfpu=auto\")
endif()"
"# ARM tuning: only add -mcpu=native -mfpu=auto on toolchains that accept
# them. Probing the flag avoids spawning grep and handles arm64 Apple clang
# (which rejects -mfpu=auto) cleanly.
if (NOT MSVC)
\tinclude(CheckCCompilerFlag)
\tcheck_c_compiler_flag(\"-mcpu=native\" SONDEDUMP_HAS_MCPU_NATIVE)
\tcheck_c_compiler_flag(\"-mfpu=auto\" SONDEDUMP_HAS_MFPU_AUTO)
\tif (SONDEDUMP_HAS_MCPU_NATIVE AND SONDEDUMP_HAS_MFPU_AUTO)
\t\tset(CMAKE_C_FLAGS_RELEASE \"\${CMAKE_C_FLAGS_RELEASE} -mcpu=native -mfpu=auto\")
\tendif()
endif()"
)

# --- Patch 2: SOURCE_BASE_PATH_SIZE uses the wrong root in sub-builds ---
patch_replace_or_fail(_content
"string(LENGTH \"\${CMAKE_SOURCE_DIR}/\" SOURCE_BASE_PATH_SIZE)"
"string(LENGTH \"\${CMAKE_CURRENT_SOURCE_DIR}/\" SOURCE_BASE_PATH_SIZE)"
)

# --- Patch 3: drop the sondedump reference executable ---
patch_replace_or_fail(_content
"# Main executable target
add_executable(sondedump \${EXEC_SOURCES})
target_include_directories(sondedump PRIVATE \${INC_DIRS}
\t\${PORTAUDIO_INCLUDE_DIRS}
\t\${CURSES_INCLUDE_DIRS}
)
target_link_libraries(sondedump PRIVATE radiosonde
\t\${CMAKE_THREAD_LIBS_INIT}
\t\${MATH_LIBRARY}
\t\${PORTAUDIO_LIBRARIES}
\t\${CURSES_LIBRARIES}
)

# Install targets
install(TARGETS sondedump DESTINATION bin)"
"# sondedump reference executable disabled by SDRIAK deps build."
)

# --- Patch 4: install rules for the radiosonde library + public headers ---
# Append once; the marker makes the append idempotent across re-runs.
set(_sdrpp_install_marker "# SDRIAK deps install rules begin")
string(FIND "${_content}" "${_sdrpp_install_marker}" _marker_idx)
if (_marker_idx LESS 0)
    string(APPEND _content "
${_sdrpp_install_marker}
install(TARGETS radiosonde
\tARCHIVE DESTINATION lib
\tLIBRARY DESTINATION lib
\tRUNTIME DESTINATION bin)
file(GLOB _SDRPP_SONDEDUMP_PUBLIC_HEADERS \"\${CMAKE_CURRENT_SOURCE_DIR}/include/*.h\")
install(FILES \${_SDRPP_SONDEDUMP_PUBLIC_HEADERS} DESTINATION include/sondedump)
# SDRIAK deps install rules end
")
endif ()

# --- Patch 5: fixed-width RS41 serial field must not be read as a C string ---
# status->serial is an 8-byte packed frame field without a NUL terminator. The
# destination is larger, so copying sizeof(dst->serial)-1 bytes with strncpy
# reads beyond the source object and trips Android's _FORTIFY_SOURCE checks.
# Copy exactly the source field; rs41.c already writes dst->serial[8] = 0 on
# the following line.
patch_apply_git_or_fail("${SRC}"
    "${CMAKE_CURRENT_LIST_DIR}/0001-fix-rs41-serial-overread.patch")

file(WRITE "${_f}" "${_content}")
message(STATUS "Patched ${_f}")
