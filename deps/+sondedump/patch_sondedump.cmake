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
#      SDR++ only consumes the `radiosonde` static library.
#   4. Adds install rules for the radiosonde library and the public API
#      headers under include/sondedump/.
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
"# sondedump reference executable disabled by SDR++ deps build."
)

# --- Patch 4: install rules for the radiosonde library + public headers ---
# Append once; the marker makes the append idempotent across re-runs.
set(_sdrpp_install_marker "# SDR++ deps install rules begin")
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
# SDR++ deps install rules end
")
endif ()

file(WRITE "${_f}" "${_content}")
message(STATUS "Patched ${_f}")

# --- Patch 5: rs41.c serial strncpy over-read (SIGABRT under _FORTIFY_SOURCE) ---
# status->serial is an 8-byte packed frame field (RS41_SERIAL_LEN) with no NUL
# terminator, but the copy length was sizeof(dst->serial)-1 == 31, so strncpy
# reads past the 8-byte source. Benign on x86 without fortify, but Android
# builds with -D_FORTIFY_SOURCE=2, whose __strncpy_chk2 aborts on the over-read
# (SIGABRT in rs41_decode). Copy the source's own size instead; the existing
# dst->serial[8] = 0 on the next line still NUL-terminates it.
set(_rs41 "${SRC}/sonde/rs41/rs41.c")
if (NOT EXISTS "${_rs41}")
    message(FATAL_ERROR "patch_sondedump: ${_rs41} not found")
endif ()
file(READ "${_rs41}" _rs41c)
string(REPLACE "\r\n" "\n" _rs41c "${_rs41c}")
patch_replace_or_fail(_rs41c
"strncpy(dst->serial, status->serial, sizeof(dst->serial)-1);"
"strncpy(dst->serial, status->serial, sizeof(status->serial));"
)
file(WRITE "${_rs41}" "${_rs41c}")
message(STATUS "Patched ${_rs41}")
