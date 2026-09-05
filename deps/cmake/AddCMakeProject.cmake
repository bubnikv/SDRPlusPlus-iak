#
# add_cmake_project(<name> ...) — wrapper around ExternalProject_Add that
# forwards toolchain/compiler/build-type, sets up parallel builds, and
# directs install into the shared destdir prefix.
#
# Same signature as ExternalProject_Add except INSTALL_DIR is managed for you.
# BUILD_COMMAND / INSTALL_COMMAND default to the standard CMake build/install
# steps, but may be overridden for projects with broken upstream install rules.
# Use ExternalProject_Add directly when you need full control (non-CMake build
# systems, binary extracts, etc.).
#

include(ExternalProject)
include(ProcessorCount)
include(${CMAKE_CURRENT_LIST_DIR}/DepPolicy.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/ValidateDep.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdrpp_trace_helpers.cmake)
sdrpp_init_cmake_trace_defaults("${CMAKE_BINARY_DIR}")

set(_sdrpp_deps_install_prefix_default "${CMAKE_CURRENT_BINARY_DIR}/destdir/usr/local")
if (DEFINED sdrpp_deps_DEP_INSTALL_PREFIX AND NOT DEFINED SDRPP_DEPS_INSTALL_PREFIX)
    set(_sdrpp_deps_install_prefix_default "${sdrpp_deps_DEP_INSTALL_PREFIX}")
endif ()
set(SDRPP_DEPS_INSTALL_PREFIX "${_sdrpp_deps_install_prefix_default}"
    CACHE PATH "Where dependencies will be installed")
set(SDRPP_DEPS_COMPAT_DIR "${CMAKE_CURRENT_BINARY_DIR}/compat"
    CACHE PATH "Where dependency build-time compatibility shims will be written")
# Default to a path outside the per-config build tree so Debug / RelWithDebInfo
# / Release builds on the same machine reuse one set of downloaded archives
# instead of fetching the same tarballs three times.
set(${PROJECT_NAME}_DEP_DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/.pkg_cache"
    CACHE PATH "Where downloaded source archives are cached")
option(${PROJECT_NAME}_DEP_BUILD_VERBOSE "Verbose output from each dependency build" OFF)

get_property(_is_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if (NOT CMAKE_BUILD_TYPE AND NOT _is_multi)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Release")
endif ()

function(sdrpp_collect_android_cmake_args out_var)
    set(_args "")
    foreach (_var IN ITEMS
            ANDROID_ABI
            ANDROID_PLATFORM
            ANDROID_STL
            ANDROID_ARM_MODE
            ANDROID_ARM_NEON)
        if (DEFINED ${_var} AND NOT "${${_var}}" STREQUAL "")
            list(APPEND _args "-D${_var}:STRING=${${_var}}")
        endif ()
    endforeach ()
    set(${out_var} ${_args} PARENT_SCOPE)
endfunction()

function(sdrpp_collect_cmake_generator_args out_var)
    # Make the child configure use the same generator family, architecture,
    # toolset, and instance as the parent deps build.
    set(_args "")
    if (CMAKE_GENERATOR)
        list(APPEND _args -G "${CMAKE_GENERATOR}")
    endif ()
    if (CMAKE_GENERATOR_PLATFORM)
        list(APPEND _args -A "${CMAKE_GENERATOR_PLATFORM}")
    endif ()
    if (CMAKE_GENERATOR_TOOLSET)
        list(APPEND _args -T "${CMAKE_GENERATOR_TOOLSET}")
    endif ()
    if (CMAKE_GENERATOR_INSTANCE)
        list(APPEND _args "-DCMAKE_GENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}")
    endif ()
    set(${out_var} ${_args} PARENT_SCOPE)
endfunction()

function(add_cmake_project projectname)
    cmake_parse_arguments(P_ARGS "" "SOURCE_SUBDIR" "CMAKE_ARGS;BUILD_COMMAND;INSTALL_COMMAND" ${ARGN})

    sdrpp_resolve_dep_policy(${projectname} _policy)
    if (_policy_BUILDS_SHARED)
        set(_project_build_shared_libs ON)
    else ()
        set(_project_build_shared_libs OFF)
    endif ()

    set(_pcount ${DEP_${projectname}_MAX_THREADS})
    if (NOT _pcount)
        set(_pcount ${DEP_MAX_THREADS})
    endif ()
    if (NOT _pcount)
        ProcessorCount(_pcount)
    endif ()
    if (_pcount EQUAL 0)
        set(_pcount 1)
    endif ()

    set(_build_j "-j${_pcount}")
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        set(_build_j "-m:${_pcount}")
    endif ()

    string(TOUPPER "${CMAKE_BUILD_TYPE}" _build_type_upper)
    set(_configs_line -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE})
    if (_is_multi)
        set(_configs_line "")
    endif ()

    set(_verbose_switch "")
    if (${PROJECT_NAME}_DEP_BUILD_VERBOSE)
        if (CMAKE_GENERATOR MATCHES "Ninja")
            set(_verbose_switch "--verbose")
        elseif (CMAKE_GENERATOR MATCHES "Visual Studio")
            set(_verbose_switch "-v:d")
        endif ()
    endif ()

    # Make generator propagation explicit so cross-arch (e.g. Win32 → x64,
    # x64 → ARM64) and non-default toolsets (e.g. v143) propagate reliably to
    # every sub-build. Per-dep compiler overrides (e.g. codec2 forcing
    # clang-cl) go through CMAKE_ARGS in the recipe and override the parent's
    # forwarded compiler.
    sdrpp_collect_cmake_generator_args(_gen_args)

    sdrpp_collect_android_cmake_args(_android_args)

    set(_cmake_args
        -DCMAKE_INSTALL_PREFIX:STRING=${SDRPP_DEPS_INSTALL_PREFIX}
        -DCMAKE_PREFIX_PATH:STRING=${SDRPP_DEPS_INSTALL_PREFIX}
        -DCMAKE_DEBUG_POSTFIX:STRING=${CMAKE_DEBUG_POSTFIX}
        ${_android_args}
        -DSDRPP_DEP_RESOLVED_PROFILE:STRING=${_policy_PROFILE}
        -DSDRPP_DEP_RESOLVED_SOURCE:STRING=${_policy_SOURCE}
        -DSDRPP_DEP_RESOLVED_LINKAGE:STRING=${_policy_LINKAGE}
        -DSDRPP_DEP_RESOLVED_USAGE:STRING=${_policy_USAGE}
        -DSDRPP_DEP_BUILDS_SHARED:BOOL=${_policy_BUILDS_SHARED}
        -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
        -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE})
    list(APPEND _cmake_args
        # CMake 4.x dropped support for projects requesting cmake_minimum_required
        # < 3.5. Many of our deps (FFTW 3.3.10, libxml2, codec2, ...) still ask
        # for older minimums. CMAKE_POLICY_VERSION_MINIMUM tells CMake to act as
        # if the minimum were 3.5 even if the project asks for less.
        -DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5)
    if (MSVC)
        list(APPEND _cmake_args
            -DCMAKE_POLICY_DEFAULT_CMP0091:STRING=NEW)
    endif ()
    if (_configs_line)
        list(APPEND _cmake_args "${_configs_line}")
    endif ()
    list(APPEND _cmake_args
        ${DEP_CMAKE_OPTS}
        ${P_ARGS_CMAKE_ARGS}
        # Keep the central policy authoritative if a recipe accidentally
        # forwards BUILD_SHARED_LIBS in its own CMAKE_ARGS.
        -DBUILD_SHARED_LIBS:BOOL=${_project_build_shared_libs})

    # If policy resolved this dep to a system-provided source (e.g. distro
    # builds taking glfw3 from libglfw3-dev), define the ExternalProject target
    # for graph-completeness but keep it out of the default ALL build — we'll
    # find_package() it against the host instead. Without this, ExternalProject
    # builds it anyway and trips over build-time deps (Wayland/xkbcommon) the
    # host package already encapsulates.
    set(_exclude_from_all FALSE)
    if (_policy_SOURCE STREQUAL "system")
        set(_exclude_from_all TRUE)
    endif ()

    set(_source_dir "<SOURCE_DIR>")
    if (P_ARGS_SOURCE_SUBDIR)
        set(_source_dir "<SOURCE_DIR>/${P_ARGS_SOURCE_SUBDIR}")
    endif ()
    set(_source_subdir_arg "")
    if (P_ARGS_SOURCE_SUBDIR)
        list(APPEND _source_subdir_arg SOURCE_SUBDIR "${P_ARGS_SOURCE_SUBDIR}")
    endif ()

    set(_configure_command ${CMAKE_COMMAND} -S "${_source_dir}" -B "<BINARY_DIR>" ${_gen_args} ${_cmake_args})
    sdrpp_wrap_traced_command(_configure_command "dep_${projectname}:configure" "${_source_dir}" ${_configure_command})

    set(_uses_default_build_command TRUE)
    set(_build_command ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} -- ${_build_j} ${_verbose_switch})
    if (P_ARGS_BUILD_COMMAND)
        set(_uses_default_build_command FALSE)
        set(_build_command ${P_ARGS_BUILD_COMMAND})
    endif ()
    if (_uses_default_build_command)
        sdrpp_wrap_traced_command(_build_command "dep_${projectname}:build" "<BINARY_DIR>" ${_build_command})
    endif ()

    set(_uses_default_install_command TRUE)
    set(_install_command ${CMAKE_COMMAND} --build . --target install --config ${CMAKE_BUILD_TYPE})
    if (P_ARGS_INSTALL_COMMAND)
        set(_uses_default_install_command FALSE)
        set(_install_command ${P_ARGS_INSTALL_COMMAND})
    endif ()
    if (_uses_default_install_command)
        sdrpp_wrap_traced_command(_install_command "dep_${projectname}:install" "<BINARY_DIR>" ${_install_command})
    endif ()
    list(APPEND _install_command
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${SDRPP_DEPS_INSTALL_PREFIX}/lib/pkgconfig
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${SDRPP_DEPS_INSTALL_PREFIX}/share/pkgconfig)

    ExternalProject_Add(
        dep_${projectname}
        INSTALL_DIR     ${SDRPP_DEPS_INSTALL_PREFIX}
        DOWNLOAD_DIR    ${${PROJECT_NAME}_DEP_DOWNLOAD_DIR}/${projectname}
        BINARY_DIR      ${CMAKE_CURRENT_BINARY_DIR}/builds/${projectname}
        EXCLUDE_FROM_ALL ${_exclude_from_all}
        ${_source_subdir_arg}
        ${P_ARGS_UNPARSED_ARGUMENTS}
        CONFIGURE_COMMAND ${_configure_command}
        BUILD_COMMAND   ${_build_command}
        INSTALL_COMMAND ${_install_command}
        USES_TERMINAL_CONFIGURE ${SDRPP_SERIALIZE_CMAKE_INVOCATIONS}
        USES_TERMINAL_BUILD ${SDRPP_SERIALIZE_CMAKE_INVOCATIONS}
        USES_TERMINAL_INSTALL ${SDRPP_SERIALIZE_CMAKE_INVOCATIONS}
    )

    set_property(TARGET dep_${projectname} PROPERTY SDRPP_RESOLVED_SOURCE "${_policy_SOURCE}")
    set_property(TARGET dep_${projectname} PROPERTY SDRPP_RESOLVED_LINKAGE "${_policy_LINKAGE}")
    set_property(TARGET dep_${projectname} PROPERTY SDRPP_RESOLVED_USAGE "${_policy_USAGE}")
endfunction()

#
# sdrpp_emit_imported_config(<name>
#     [TARGET <imported-target>]
#     [LIB_NAMES <names...>]
#     [STATIC_LIB_NAMES <names...>]
#     [SHARED_LIB_NAMES <names...>]
#     [DLL_NAMES <names...>]
#     [HEADER <relative/path.h>]
#     [INCLUDE_SUBDIR <subdir>])
#
# Writes a small Config.cmake into the deps install prefix that resolves an
# imported library at consume time via find_library / find_file / find_path
# against the prefix. Use for libraries that don't ship Config.cmake upstream
# (most Alex Rouma forks, hydrasdr, libad9361, sdrplay, libusb,
# etc.). Does NOT replace upstream Config.cmake — if upstream installs one,
# don't call this helper.
#
# Defaults:
#   TARGET         ${name}::${name}
#   LIB_NAMES      ${name} with leading "lib" stripped, plus full ${name}
#   DLL_NAMES      same as LIB_NAMES with .dll suffix added
#   HEADER         (none — relies on INTERFACE_INCLUDE_DIRECTORIES = root/include)
#   INCLUDE_SUBDIR (none — uses root/include directly)
#
function(sdrpp_emit_imported_config name)
    cmake_parse_arguments(P "" "TARGET;HEADER;INCLUDE_SUBDIR"
        "LIB_NAMES;STATIC_LIB_NAMES;SHARED_LIB_NAMES;DLL_NAMES;COMPILE_DEFINITIONS;LINK_LIBRARIES;PACKAGE_DEPENDENCIES" ${ARGN})

    sdrpp_resolve_dep_policy(${name} _policy)

    set(_target ${P_TARGET})
    if (NOT _target)
        set(_target ${name}::${name})
    endif ()

    if (_policy_BUILDS_SHARED AND P_SHARED_LIB_NAMES)
        set(_lib_names ${P_SHARED_LIB_NAMES})
    elseif (NOT _policy_BUILDS_SHARED AND P_STATIC_LIB_NAMES)
        set(_lib_names ${P_STATIC_LIB_NAMES})
    elseif (P_LIB_NAMES)
        set(_lib_names ${P_LIB_NAMES})
    else ()
        string(REGEX REPLACE "^lib" "" _short ${name})
        set(_lib_names ${_short} ${name})
    endif ()
    list(REMOVE_DUPLICATES _lib_names)

    if (P_DLL_NAMES)
        set(_dll_names ${P_DLL_NAMES})
    else ()
        set(_dll_names "")
        foreach (l ${_lib_names})
            list(APPEND _dll_names "${l}.dll")
        endforeach ()
    endif ()

    # In Debug deps trees on MSVC, upstream CMakeLists that hardcode
    # CMAKE_DEBUG_POSTFIX=d install libs with a 'd' suffix. The emitted
    # Config.cmake will be consumed by an app build that also targets Debug
    # (single-config trees match end-to-end), so prepend the d-postfixed
    # variants. find_library/find_file return the first existing match — libs
    # that don't apply the postfix still resolve via the plain name.
    if (MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_postfixed_libs "")
        foreach (n ${_lib_names})
            list(APPEND _postfixed_libs "${n}d")
        endforeach ()
        list(PREPEND _lib_names ${_postfixed_libs})
        list(REMOVE_DUPLICATES _lib_names)

        set(_postfixed_dlls "")
        foreach (dll ${_dll_names})
            get_filename_component(_stem "${dll}" NAME_WE)
            get_filename_component(_ext "${dll}" EXT)
            list(APPEND _postfixed_dlls "${_stem}d${_ext}")
        endforeach ()
        list(PREPEND _dll_names ${_postfixed_dlls})
        list(REMOVE_DUPLICATES _dll_names)
    endif ()

    set(_inc_dir "\${_root}/include")
    if (P_INCLUDE_SUBDIR)
        set(_inc_dir "\${_root}/include/${P_INCLUDE_SUBDIR}")
    endif ()

    set(_lib_names_str "${_lib_names}")
    string(REPLACE ";" " " _lib_names_str "${_lib_names_str}")
    set(_dll_names_str "${_dll_names}")
    string(REPLACE ";" " " _dll_names_str "${_dll_names_str}")

    set(_dest_dir "${SDRPP_DEPS_INSTALL_PREFIX}/lib/cmake/${name}")
    set(_dest "${_dest_dir}/${name}Config.cmake")

    set(_imported_type SHARED)
    if (NOT _policy_BUILDS_SHARED)
        set(_imported_type STATIC)
    endif ()

    # NO_CACHE was added in CMake 3.21. Consumers may be on older CMake
    # (Ubuntu focal ships 3.16), so emit a runtime gate that uses NO_CACHE
    # when available and falls back to the regular cache lookup otherwise.
    set(_content "# Generated by SDRIAK deps build for ${name}\n")
    string(APPEND _content "get_filename_component(_root \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n")
    string(APPEND _content "if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)\n")
    string(APPEND _content "    set(_sdrpp_no_cache NO_CACHE)\n")
    string(APPEND _content "else ()\n")
    string(APPEND _content "    set(_sdrpp_no_cache \"\")\n")
    string(APPEND _content "endif ()\n")
    # NO_CMAKE_FIND_ROOT_PATH bypasses cross-toolchain root-path filtering
    # (Android NDK sets CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY, which would
    # otherwise reject the explicit PATHS for living outside the NDK sysroot).
    # Same rationale as the consumer-side fix in cmake/sdrpp_find_dep.cmake.
    foreach (_dependency ${P_PACKAGE_DEPENDENCIES})
        string(APPEND _content "find_package(${_dependency} CONFIG REQUIRED PATHS \"\${_root}\" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)\n")
    endforeach ()
    string(APPEND _content "if (NOT TARGET ${_target})\n")
    string(APPEND _content "    add_library(${_target} ${_imported_type} IMPORTED)\n")
    # When both static (.a) and shared (.so/.dylib) variants live in the same
    # install dir, find_library's default suffix order (.so;.a on Linux,
    # .dylib;.so;.a on macOS) picks shared first regardless of names — because
    # it iterates suffixes for every name before moving to the next name. We
    # need find_library to honor the policy-resolved linkage, so constrain
    # CMAKE_FIND_LIBRARY_SUFFIXES around the call. Windows already uses .lib
    # for both static and import libs, so the naming convention (e.g.
    # *_static.lib) is what disambiguates there — leave the default suffix.
    if (NOT _policy_BUILDS_SHARED)
        string(APPEND _content "    set(_sdrpp_saved_suffixes \"\${CMAKE_FIND_LIBRARY_SUFFIXES}\")\n")
        string(APPEND _content "    if (NOT WIN32)\n")
        string(APPEND _content "        set(CMAKE_FIND_LIBRARY_SUFFIXES \".a\")\n")
        string(APPEND _content "    endif ()\n")
    else ()
        # Constrain find_library to shared-library suffixes so a co-installed
        # static archive (libfoo.a alongside libfoo.dylib / libfoo.so) doesn't
        # latch onto the wrong artifact. On Apple we also keep .so in the
        # list because SDRplay's macOS binary blob uses the .so extension
        # instead of the conventional .dylib — without it find_library inside
        # the emitted sdrplayConfig.cmake misses the install we just laid down.
        string(APPEND _content "    set(_sdrpp_saved_suffixes \"\${CMAKE_FIND_LIBRARY_SUFFIXES}\")\n")
        string(APPEND _content "    if (APPLE)\n")
        string(APPEND _content "        set(CMAKE_FIND_LIBRARY_SUFFIXES \".dylib;.so\")\n")
        string(APPEND _content "    elseif (UNIX)\n")
        string(APPEND _content "        set(CMAKE_FIND_LIBRARY_SUFFIXES \".so\")\n")
        string(APPEND _content "    endif ()\n")
    endif ()
    # HINTS are searched before default paths; drop NO_DEFAULT_PATH so the
    # emitted Config falls back to system search when the dep wasn't bundled
    # into the prefix (distro profile with source=system). The PATH_SUFFIXES
    # entry covers multi-arch system include dirs (/usr/include/libusb-1.0
    # etc.) when find_path falls through to defaults.
    #
    # NO_CMAKE_FIND_ROOT_PATH bypasses cross-toolchain root-path filtering
    # (Android NDK sets CMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY and
    # CMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY, which would otherwise restrict
    # find_library / find_path to the NDK sysroot and miss our deps prefix
    # entirely, yielding IMPORTED_LOCATION="-NOTFOUND" and
    # INTERFACE_INCLUDE_DIRECTORIES="_<name>_inc-NOTFOUND" on the imported
    # target). No-op on native toolchains that don't set CMAKE_FIND_ROOT_PATH.
    string(APPEND _content "    find_library(_${name}_imp NAMES ${_lib_names_str} HINTS \"\${_root}/lib\" \"\${_root}/bin\" NO_CMAKE_FIND_ROOT_PATH \${_sdrpp_no_cache})\n")
    string(APPEND _content "    set(CMAKE_FIND_LIBRARY_SUFFIXES \"\${_sdrpp_saved_suffixes}\")\n")
    if (P_HEADER)
        set(_path_suffixes_arg "")
        if (P_INCLUDE_SUBDIR)
            set(_path_suffixes_arg "PATH_SUFFIXES ${P_INCLUDE_SUBDIR}")
        endif ()
        string(APPEND _content "    find_path(_${name}_inc \"${P_HEADER}\" HINTS \"${_inc_dir}\" \"\${_root}/include\" ${_path_suffixes_arg} NO_CMAKE_FIND_ROOT_PATH \${_sdrpp_no_cache})\n")
    endif ()
    if (_policy_BUILDS_SHARED)
        string(APPEND _content "    if (WIN32)\n")
        string(APPEND _content "        find_file(_${name}_loc NAMES ${_dll_names_str} HINTS \"\${_root}/bin\" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH \${_sdrpp_no_cache})\n")
        string(APPEND _content "        set_target_properties(${_target} PROPERTIES IMPORTED_LOCATION \"\${_${name}_loc}\" IMPORTED_IMPLIB \"\${_${name}_imp}\")\n")
        string(APPEND _content "    else ()\n")
        string(APPEND _content "        set_target_properties(${_target} PROPERTIES IMPORTED_LOCATION \"\${_${name}_imp}\")\n")
        string(APPEND _content "    endif ()\n")
    else ()
        string(APPEND _content "    set_target_properties(${_target} PROPERTIES IMPORTED_LOCATION \"\${_${name}_imp}\")\n")
    endif ()
    if (P_HEADER)
        # Emit BOTH the resolved include dir AND the prefix's include root so
        # consumers can include the header either way:
        #   #include <libusb.h>          (needs /include/libusb-1.0 in path)
        #   #include <librfnm/librfnm.h> (needs /include in path)
        # Both forms appear in this codebase; emitting both dirs keeps the
        # Config consumer-agnostic. CMake dedups, so the no-subdir case
        # where _<name>_inc already == _root/include costs nothing.
        string(APPEND _content "    set_target_properties(${_target} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES \"\${_${name}_inc};\${_root}/include\")\n")
    else ()
        string(APPEND _content "    set_target_properties(${_target} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES \"${_inc_dir};\${_root}/include\")\n")
    endif ()
    if (P_COMPILE_DEFINITIONS)
        set(_compile_definitions "${P_COMPILE_DEFINITIONS}")
        string(APPEND _content "    set_target_properties(${_target} PROPERTIES INTERFACE_COMPILE_DEFINITIONS \"${_compile_definitions}\")\n")
    endif ()
    if (P_LINK_LIBRARIES)
        set(_link_libraries "${P_LINK_LIBRARIES}")
        string(APPEND _content "    set_target_properties(${_target} PROPERTIES INTERFACE_LINK_LIBRARIES \"${_link_libraries}\")\n")
    endif ()
    string(APPEND _content "endif ()\n")

    file(MAKE_DIRECTORY "${_dest_dir}")
    file(WRITE "${_dest}" "${_content}")

    # Auto-validate the install we just promised to consume. The validator
    # checks both the selected artifacts and the imported target properties,
    # so policy mismatches surface before the app tries to link them.
    # Skipped for system-resolved deps — the host provides the library, the
    # deps prefix has nothing to validate, and the emitted Config's
    # system-fallback search will resolve the target at consume time.
    if (NOT "${_policy_SOURCE}" STREQUAL "system")
        sdrpp_validate_dep(${name}
            TARGET         ${_target}
            LIB_NAMES      ${_lib_names}
            DLL_NAMES      ${_dll_names}
            HEADER         ${P_HEADER}
            INCLUDE_SUBDIR ${P_INCLUDE_SUBDIR}
            REQUIRES_CONFIG)
    endif ()
endfunction()
