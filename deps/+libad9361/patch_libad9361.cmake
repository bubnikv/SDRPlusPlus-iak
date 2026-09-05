#
# Run with -DSRC=<source-dir> -P this-script.
#
# libad9361 v0.2 has only export/import modes in its public header. SDRIAK uses
# the bundled lib as a static module-private dependency on portable builds, so
# add an explicit LIBAD9361_STATIC mode and define it on the library target.
# Disable upstream's framework output too: the deps prefix and generated
# package config consume a normal libad9361 archive or shared library.
#
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/patch_helpers.cmake)

set(_cmake "${SRC}/CMakeLists.txt")
file(READ "${_cmake}" _content)
patch_replace_or_fail(_content
    "target_link_libraries(ad9361 LINK_PRIVATE \${LIBIIO_LIBRARIES})"
    "if (NOT BUILD_SHARED_LIBS)\n\ttarget_compile_definitions(ad9361 PRIVATE LIBAD9361_STATIC LIBIIO_STATIC)\nendif()\ntarget_link_libraries(ad9361 LINK_PRIVATE \${LIBIIO_LIBRARIES})")
patch_replace_or_fail(_content
    "\tFRAMEWORK TRUE"
    "\tFRAMEWORK FALSE")
file(WRITE "${_cmake}" "${_content}")
message(STATUS "Patched ${_cmake}")

set(_header "${SRC}/ad9361.h")
file(READ "${_header}" _header_content)
patch_replace_or_fail(_header_content
    "#   ifdef LIBAD9361_EXPORTS\n#\tdefine __api __declspec(dllexport)\n#   else\n#\tdefine __api __declspec(dllimport)\n#   endif"
    "#   ifdef LIBAD9361_STATIC\n#\tdefine __api\n#   elif defined(LIBAD9361_EXPORTS)\n#\tdefine __api __declspec(dllexport)\n#   else\n#\tdefine __api __declspec(dllimport)\n#   endif")
file(WRITE "${_header}" "${_header_content}")
message(STATUS "Patched ${_header}")
