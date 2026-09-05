#
# Run with -DSRC=<source-dir> -P this-script.
#
# libhydrasdr (hydrasdr/hydrasdr-host) embeds a hydrasdr-tools sub-project
# whose internal FetchContent pthreads4w uses the static MSVC CRT while the
# parent project uses the DLL CRT, causing unresolved __imp__beginthreadex etc.
# at link time. SDRIAK only needs the library target, so skip the tools
# subdirectory entirely.
#
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/patch_helpers.cmake)

set(_f "${SRC}/CMakeLists.txt")
file(READ "${_f}" _content)
patch_replace_or_fail(_content
    "add_subdirectory(hydrasdr-tools)"
    "# hydrasdr-tools subdirectory disabled by SDRIAK deps build (tools not needed)")
file(WRITE "${_f}" "${_content}")
message(STATUS "Patched ${_f}")

set(_lib_cmake "${SRC}/libhydrasdr/CMakeLists.txt")
if (EXISTS "${_lib_cmake}")
    file(READ "${_lib_cmake}" _lib_cmake_content)
    patch_replace_or_fail(_lib_cmake_content
[=[set_target_properties(hydrasdr PROPERTIES
    VERSION ${HYDRASDR_VER_MAJOR}.${HYDRASDR_VER_MINOR}.${HYDRASDR_VER_REVISION}
    SOVERSION ${HYDRASDR_VER_MAJOR}
)]=]
[=[if(TARGET hydrasdr)
  set_target_properties(hydrasdr PROPERTIES
    VERSION ${HYDRASDR_VER_MAJOR}.${HYDRASDR_VER_MINOR}.${HYDRASDR_VER_REVISION}
    SOVERSION ${HYDRASDR_VER_MAJOR}
  )
endif()]=])
    file(WRITE "${_lib_cmake}" "${_lib_cmake_content}")
    message(STATUS "Patched ${_lib_cmake}")
endif ()

set(_h "${SRC}/libhydrasdr/src/hydrasdr.h")
file(READ "${_h}" _header)
patch_replace_or_fail(_header
    "	#ifdef ADD_EXPORTS\n		#define ADDAPI __declspec(dllexport)\n	#else\n		#define ADDAPI __declspec(dllimport)\n	#endif"
    "	#ifdef HYDRASDR_STATIC\n		#define ADDAPI\n	#elif defined(ADD_EXPORTS)\n		#define ADDAPI __declspec(dllexport)\n	#else\n		#define ADDAPI __declspec(dllimport)\n	#endif")
file(WRITE "${_h}" "${_header}")
message(STATUS "Patched ${_h}")

#
# Streaming-teardown crash fix, kept as a proper git patch because it is
# intended for an upstream pull request (github.com/hydrasdr/hydrasdr-host).
#
# The teardown frees USB transfers after a blind 2 x 50 ms event drain with
# no in-flight accounting; un-reaped cancellations (guaranteed on Android
# when the USB fd is closed on device detach) leave freed nodes on libusb's
# flying-transfers list and crash inside libusb_close(). See the patch header
# for the full analysis.
#
patch_apply_git_or_fail("${SRC}" "${CMAKE_CURRENT_LIST_DIR}/0001-fix-use-after-free-in-streaming-teardown.patch")
