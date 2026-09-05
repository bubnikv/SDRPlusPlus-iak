#
# Run with -DSRC=<source-dir> -P this-script.
#

if (NOT DEFINED SRC)
    message(FATAL_ERROR "patch_codec2: SRC is not set")
endif ()

find_package(Git REQUIRED)

set(_codec2_patches
    "${CMAKE_CURRENT_LIST_DIR}/patches/0001-sdriak-windows-clangcl-build.patch")

foreach (_patch IN LISTS _codec2_patches)
    if (NOT EXISTS "${_patch}")
        message(FATAL_ERROR "patch_codec2: missing patch ${_patch}")
    endif ()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SRC}" apply
            --check --ignore-whitespace --whitespace=nowarn "${_patch}"
        RESULT_VARIABLE _check_result
        OUTPUT_VARIABLE _check_output
        ERROR_VARIABLE _check_error)

    if (_check_result EQUAL 0)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${SRC}" apply
                --ignore-whitespace --whitespace=nowarn "${_patch}"
            RESULT_VARIABLE _apply_result
            OUTPUT_VARIABLE _apply_output
            ERROR_VARIABLE _apply_error)
        if (NOT _apply_result EQUAL 0)
            message(FATAL_ERROR
                "patch_codec2: failed applying ${_patch}\n"
                "${_apply_output}${_apply_error}")
        endif ()
        message(STATUS "patch_codec2: applied ${_patch}")
    else ()
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${SRC}" apply
                --check --reverse --ignore-whitespace --whitespace=nowarn "${_patch}"
            RESULT_VARIABLE _reverse_check_result
            OUTPUT_VARIABLE _reverse_check_output
            ERROR_VARIABLE _reverse_check_error)

        if (_reverse_check_result EQUAL 0)
            message(STATUS "patch_codec2: ${_patch} already applied")
        else ()
            message(FATAL_ERROR
                "patch_codec2: ${_patch} does not apply cleanly\n"
                "Forward check:\n${_check_output}${_check_error}\n"
                "Reverse check:\n${_reverse_check_output}${_reverse_check_error}")
        endif ()
    endif ()
endforeach ()

#
# Additional string-replacement patch — insert a GENERATE_CODEBOOK branch at
# the top of the if(CMAKE_CROSSCOMPILING)/else() chain in src/CMakeLists.txt
# so the recipe can supply a pre-built native helper exe and short-circuit
# both the nested ExternalProject (which inherits the cross-target compiler
# and thus also fails) and the native build (which produces an unrunnable
# target-arch helper). Mirrors the -DGENERATE_CODEBOOK=... pattern used by
# the Android cross-build script.
#
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/patch_helpers.cmake)

set(_src_cmakelists "${SRC}/src/CMakeLists.txt")
file(READ "${_src_cmakelists}" _src_content)
patch_replace_or_fail(_src_content
"# when crosscompiling we need a native executable
if(CMAKE_CROSSCOMPILING)"
"# when crosscompiling we need a native executable
if(GENERATE_CODEBOOK)
    # Pre-built native helper supplied by the caller (e.g. SDRIAK deps
    # build when cross-compiling x64 host -> ARM64 target). Avoids the
    # nested ExternalProject below, which would inherit the cross-target
    # compiler env and produce an equally un-runnable helper exe.
    add_executable(generate_codebook IMPORTED)
    set_target_properties(generate_codebook PROPERTIES
        IMPORTED_LOCATION \"\${GENERATE_CODEBOOK}\")
elseif(CMAKE_CROSSCOMPILING)")
# Drop the legacy if/endif labels — they referred to the original
# if(CMAKE_CROSSCOMPILING), but with the new top branch above CMake warns
# about mis-matched block arguments. else()/endif() with no labels match
# any open block.
patch_replace_or_fail(_src_content
    "else(CMAKE_CROSSCOMPILING)"
    "else()")
patch_replace_or_fail(_src_content
    "endif(CMAKE_CROSSCOMPILING)"
    "endif()")
file(WRITE "${_src_cmakelists}" "${_src_content}")
message(STATUS "patch_codec2: inserted GENERATE_CODEBOOK branch in ${_src_cmakelists}")
