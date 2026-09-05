#
# Central policy helpers for SDRIAK third-party dependencies.
#
# This layer resolves three orthogonal per-package attributes:
#   - source origin  : bundled | system | auto
#   - linkage mode   : static | shared | header-only | auto
#   - usage class    : module-private | shared-runtime | core
#
# Classified dependencies use explicit per-profile defaults. Unclassified
# dependencies still fall back to the legacy SYSTEM_PROVIDED_PACKAGES +
# BUILD_SHARED_LIBS model when SDRPP_DEP_POLICY_STRICT=OFF.
#

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdrpp_dep_list_helpers.cmake")

if (COMMAND sdrpp_register_dep)
    return()
endif ()

function(_sdrpp_dep_list_contains out_var dep_name raw_value)
    sdrpp_normalize_dep_list(_list "${raw_value}")
    if ("${dep_name}" IN_LIST _list)
        set(${out_var} TRUE PARENT_SCOPE)
    else ()
        set(${out_var} FALSE PARENT_SCOPE)
    endif ()
endfunction()

function(_sdrpp_dep_get_property out_var dep_name suffix)
    get_property(_is_set GLOBAL PROPERTY "SDRPP_DEP_${dep_name}_${suffix}" SET)
    if (_is_set)
        get_property(_value GLOBAL PROPERTY "SDRPP_DEP_${dep_name}_${suffix}")
    else ()
        set(_value "")
    endif ()
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

function(_sdrpp_dep_resolve_profile_value out_var profile legacy_value)
    set(_fallback "")
    set(_resolved "")
    foreach (_entry ${ARGN})
        if (_entry MATCHES "^([A-Za-z0-9_-]+):(.+)$")
            if (CMAKE_MATCH_1 STREQUAL "${profile}")
                set(_resolved "${CMAKE_MATCH_2}")
            elseif (CMAKE_MATCH_1 STREQUAL "default" AND "${_fallback}" STREQUAL "")
                set(_fallback "${CMAKE_MATCH_2}")
            endif ()
        elseif (NOT "${_entry}" STREQUAL "")
            set(_fallback "${_entry}")
        endif ()
    endforeach ()

    if ("${_resolved}" STREQUAL "")
        if (NOT "${_fallback}" STREQUAL "")
            set(_resolved "${_fallback}")
        else ()
            set(_resolved "${legacy_value}")
        endif ()
    endif ()

    set(${out_var} "${_resolved}" PARENT_SCOPE)
endfunction()

function(sdrpp_register_dep dep_name)
    cmake_parse_arguments(P "" "USAGE" "DEFAULT_SOURCE;DEFAULT_LINKAGE" ${ARGN})

    get_property(_registered GLOBAL PROPERTY SDRPP_DEP_REGISTERED_PACKAGES)
    if (NOT "${dep_name}" IN_LIST _registered)
        set_property(GLOBAL APPEND PROPERTY SDRPP_DEP_REGISTERED_PACKAGES "${dep_name}")
    endif ()

    set_property(GLOBAL PROPERTY "SDRPP_DEP_${dep_name}_DEFAULT_SOURCE" "${P_DEFAULT_SOURCE}")
    set_property(GLOBAL PROPERTY "SDRPP_DEP_${dep_name}_DEFAULT_LINKAGE" "${P_DEFAULT_LINKAGE}")
    set_property(GLOBAL PROPERTY "SDRPP_DEP_${dep_name}_USAGE" "${P_USAGE}")
endfunction()

function(sdrpp_resolve_dep_policy dep_name out_prefix)
    if (ANDROID)
        set(_profile_default android)
    elseif (WIN32 OR APPLE)
        set(_profile_default portable)
    else ()
        set(_profile_default distro)
    endif ()

    if (DEFINED SDRPP_DEP_PROFILE AND NOT "${SDRPP_DEP_PROFILE}" STREQUAL "")
        set(_profile "${SDRPP_DEP_PROFILE}")
    else ()
        set(_profile "${_profile_default}")
    endif ()

    set(_legacy_source bundled)
    if (DEFINED SYSTEM_PROVIDED_PACKAGES AND "${dep_name}" IN_LIST SYSTEM_PROVIDED_PACKAGES)
        set(_legacy_source system)
    endif ()

    if (BUILD_SHARED_LIBS)
        set(_legacy_linkage shared)
    else ()
        set(_legacy_linkage static)
    endif ()

    get_property(_registered GLOBAL PROPERTY SDRPP_DEP_REGISTERED_PACKAGES)
    if ("${dep_name}" IN_LIST _registered)
        set(_has_metadata TRUE)
    else ()
        set(_has_metadata FALSE)
    endif ()

    _sdrpp_dep_list_contains(_force_bundled "${dep_name}" "${SDRPP_DEP_FORCE_BUNDLED}")
    _sdrpp_dep_list_contains(_force_system  "${dep_name}" "${SDRPP_DEP_FORCE_SYSTEM}")
    _sdrpp_dep_list_contains(_force_shared  "${dep_name}" "${SDRPP_DEP_FORCE_SHARED}")
    _sdrpp_dep_list_contains(_force_static  "${dep_name}" "${SDRPP_DEP_FORCE_STATIC}")

    _sdrpp_dep_get_property(_source_specs  "${dep_name}" "DEFAULT_SOURCE")
    _sdrpp_dep_get_property(_linkage_specs "${dep_name}" "DEFAULT_LINKAGE")
    _sdrpp_dep_get_property(_usage         "${dep_name}" "USAGE")

    if (SDRPP_DEP_POLICY_STRICT AND NOT _has_metadata AND NOT _force_bundled AND NOT _force_system AND NOT _force_shared AND NOT _force_static)
        message(FATAL_ERROR "Dependency '${dep_name}' has no policy metadata. Classify it with sdrpp_register_dep() or force it via SDRPP_DEP_FORCE_*.")
    endif ()

    if (_force_bundled AND _force_system)
        message(FATAL_ERROR "Dependency '${dep_name}' is listed in both SDRPP_DEP_FORCE_BUNDLED and SDRPP_DEP_FORCE_SYSTEM.")
    endif ()
    if (_force_shared AND _force_static)
        message(FATAL_ERROR "Dependency '${dep_name}' is listed in both SDRPP_DEP_FORCE_SHARED and SDRPP_DEP_FORCE_STATIC.")
    endif ()

    if (_force_bundled)
        set(_source bundled)
    elseif (_force_system)
        set(_source system)
    elseif (_has_metadata)
        _sdrpp_dep_resolve_profile_value(_source "${_profile}" "${_legacy_source}" ${_source_specs})
    else ()
        set(_source "${_legacy_source}")
    endif ()

    if (_force_shared)
        set(_linkage shared)
    elseif (_force_static)
        set(_linkage static)
    elseif (_has_metadata)
        _sdrpp_dep_resolve_profile_value(_linkage "${_profile}" "${_legacy_linkage}" ${_linkage_specs})
    else ()
        set(_linkage "${_legacy_linkage}")
    endif ()

    if ("${_usage}" STREQUAL "")
        set(_usage unspecified)
    endif ()

    set(_is_system FALSE)
    if ("${_source}" STREQUAL "system")
        set(_is_system TRUE)
    endif ()

    set(_builds_shared FALSE)
    if ("${_linkage}" STREQUAL "shared")
        set(_builds_shared TRUE)
    endif ()

    set_property(GLOBAL PROPERTY "SDRPP_DEP_${dep_name}_RESOLVED_PROFILE" "${_profile}")
    set_property(GLOBAL PROPERTY "SDRPP_DEP_${dep_name}_RESOLVED_SOURCE" "${_source}")
    set_property(GLOBAL PROPERTY "SDRPP_DEP_${dep_name}_RESOLVED_LINKAGE" "${_linkage}")
    set_property(GLOBAL PROPERTY "SDRPP_DEP_${dep_name}_RESOLVED_USAGE" "${_usage}")

    set(${out_prefix}_PROFILE "${_profile}" PARENT_SCOPE)
    set(${out_prefix}_SOURCE "${_source}" PARENT_SCOPE)
    set(${out_prefix}_LINKAGE "${_linkage}" PARENT_SCOPE)
    set(${out_prefix}_USAGE "${_usage}" PARENT_SCOPE)
    set(${out_prefix}_IS_SYSTEM ${_is_system} PARENT_SCOPE)
    set(${out_prefix}_BUILDS_SHARED ${_builds_shared} PARENT_SCOPE)
endfunction()

function(sdrpp_dep_is_system dep_name out_var)
    sdrpp_resolve_dep_policy(${dep_name} _policy)
    set(${out_var} ${_policy_IS_SYSTEM} PARENT_SCOPE)
endfunction()

function(sdrpp_dep_builds_shared dep_name out_var)
    sdrpp_resolve_dep_policy(${dep_name} _policy)
    set(${out_var} ${_policy_BUILDS_SHARED} PARENT_SCOPE)
endfunction()

function(sdrpp_dep_get_linkage_option_bools dep_name out_shared_var out_static_var)
    sdrpp_dep_builds_shared(${dep_name} _builds_shared)
    if (_builds_shared)
        set(_shared ON)
        set(_static OFF)
    else ()
        set(_shared OFF)
        set(_static ON)
    endif ()

    set(${out_shared_var} ${_shared} PARENT_SCOPE)
    set(${out_static_var} ${_static} PARENT_SCOPE)
endfunction()
