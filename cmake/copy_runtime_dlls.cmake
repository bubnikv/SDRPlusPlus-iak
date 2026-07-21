if (NOT DEFINED RUNTIME_MANIFEST OR NOT DEFINED DST)
    message(FATAL_ERROR "copy_runtime_dlls.cmake requires -DRUNTIME_MANIFEST=<file> and -DDST=<dir>")
endif ()

# file(GET_RUNTIME_DEPENDENCIES) below normalizes paths before matching under
# CMP0207 (CMake 3.31+). NEW is behavior-safe here and silences the per-DLL
# author warnings emitted when the policy is left unset.
if (POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif ()

file(MAKE_DIRECTORY "${DST}")

include("${RUNTIME_MANIFEST}")

if (SDRPP_RUNTIME_DLLS)
    list(REMOVE_ITEM SDRPP_RUNTIME_DLLS "")
    list(REMOVE_DUPLICATES SDRPP_RUNTIME_DLLS)
endif ()

set(_runtime_names "")
foreach (_dll IN LISTS SDRPP_RUNTIME_DLLS)
    get_filename_component(_name "${_dll}" NAME)
    list(APPEND _runtime_names "${_name}")
endforeach ()
list(REMOVE_DUPLICATES _runtime_names)

foreach (_src_dir IN LISTS SDRPP_RUNTIME_SOURCE_DIRS)
    if (NOT EXISTS "${_src_dir}")
        continue()
    endif ()

    file(GLOB _source_dlls "${_src_dir}/*.dll")
    foreach (_source_dll IN LISTS _source_dlls)
        get_filename_component(_source_name "${_source_dll}" NAME)
        if (NOT "${_source_name}" IN_LIST _runtime_names AND EXISTS "${DST}/${_source_name}")
            file(REMOVE "${DST}/${_source_name}")
        endif ()
    endforeach ()
endforeach ()

foreach (_dll IN LISTS SDRPP_RUNTIME_DLLS)
    if (EXISTS "${_dll}")
        get_filename_component(_name "${_dll}" NAME)
        get_filename_component(_src_abs "${_dll}" ABSOLUTE)
        get_filename_component(_dst_abs "${DST}/${_name}" ABSOLUTE)
        if (NOT _src_abs STREQUAL _dst_abs)
            file(COPY "${_dll}" DESTINATION "${DST}")
        endif ()
    endif ()
endforeach ()

if (DEFINED RUNTIME_DEPENDENCIES_COMMAND AND EXISTS "${RUNTIME_DEPENDENCIES_COMMAND}")
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM windows+pe)
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL dumpbin)
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${RUNTIME_DEPENDENCIES_COMMAND}")
endif ()

file(GLOB _seed_executables "${DST}/*.exe")
file(GLOB _seed_libraries "${DST}/*.dll")
if (_seed_executables OR _seed_libraries)
    file(GET_RUNTIME_DEPENDENCIES
        RESOLVED_DEPENDENCIES_VAR _resolved_dependencies
        UNRESOLVED_DEPENDENCIES_VAR _unresolved_dependencies
        CONFLICTING_DEPENDENCIES_PREFIX _conflicting_dependencies
        EXECUTABLES ${_seed_executables}
        LIBRARIES ${_seed_libraries}
        DIRECTORIES ${SDRPP_RUNTIME_SOURCE_DIRS})

    foreach (_resolved_dependency IN LISTS _resolved_dependencies)
        file(TO_CMAKE_PATH "${_resolved_dependency}" _resolved_dependency_norm)
        string(TOLOWER "${_resolved_dependency_norm}" _resolved_dependency_lower)

        set(_copy_dependency FALSE)
        foreach (_src_dir IN LISTS SDRPP_RUNTIME_SOURCE_DIRS)
            file(TO_CMAKE_PATH "${_src_dir}" _src_dir_norm)
            string(TOLOWER "${_src_dir_norm}" _src_dir_lower)
            string(FIND "${_resolved_dependency_lower}" "${_src_dir_lower}/" _src_dir_match)
            if (_src_dir_match EQUAL 0)
                set(_copy_dependency TRUE)
                break()
            endif ()
        endforeach ()

        if (_copy_dependency)
            file(COPY "${_resolved_dependency}" DESTINATION "${DST}")
        endif ()
    endforeach ()
endif ()
