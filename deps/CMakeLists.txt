#
# Top-level CMake project to download, configure and build third-party
# dependencies for SDRIAK. Adapted from PrusaSlicer's deps/ subsystem.
#
# Each library lives in a +<Name>/ folder containing a single <Name>.cmake
# recipe. Adding a new dependency = drop in a new folder.
#
# Typical use:
#   cmake -S deps --preset vs2026-x64-release
#   cmake --build deps/build-vs2026-x64-Release --config Release
#
# Outputs land in <buildDir>/destdir/usr/local. Bundled deps are consumed via
# CMake config packages; pkg-config metadata is pruned from the prefix.
# Validation manifests, generated probe projects, and compatibility shims stay
# in <buildDir>/. Point the SDRIAK build at the install prefix:
#   cmake -S . -B build -G "Visual Studio 18 2026" -A x64 \
#         -DCMAKE_PREFIX_PATH=<absolute>/deps/build-vs2026-x64-Release/destdir/usr/local
#
# This is a single-config build system: each build tree produces exactly one
# configuration, determined by CMAKE_BUILD_TYPE. Debug builds link against the
# MSVC debug DLL CRT (/MDd), Release/RelWithDebInfo builds against /MD. To get
# multiple configurations on the same machine, use separate build trees driven
# from CMakePresets.json at the repo root.
#

cmake_minimum_required(VERSION 3.16...3.24)  # Ubuntu focal floor
project(sdrpp_deps)

include(${PROJECT_SOURCE_DIR}/../cmake/sdrpp_trace_helpers.cmake)
include(${PROJECT_SOURCE_DIR}/../cmake/sdrpp_dep_list_helpers.cmake)
sdrpp_init_cmake_trace_defaults("${CMAKE_CURRENT_BINARY_DIR}")

if (ANDROID)
    set(_sdrpp_dep_profile_default android)
elseif (WIN32 OR APPLE)
    set(_sdrpp_dep_profile_default portable)
else ()
    set(_sdrpp_dep_profile_default distro)
endif ()

set(SDRPP_DEP_PROFILE "${_sdrpp_dep_profile_default}" CACHE STRING "Dependency policy profile (portable|distro|android)")
set_property(CACHE SDRPP_DEP_PROFILE PROPERTY STRINGS portable distro android)
set(SDRPP_DEP_FORCE_BUNDLED "" CACHE STRING "Comma/space/semicolon-separated dependency names to force bundled")
set(SDRPP_DEP_FORCE_SYSTEM "" CACHE STRING "Comma/space/semicolon-separated dependency names to force system-provided")
set(SDRPP_DEP_FORCE_SHARED "" CACHE STRING "Comma/space/semicolon-separated dependency names to force shared linkage")
set(SDRPP_DEP_FORCE_STATIC "" CACHE STRING "Comma/space/semicolon-separated dependency names to force static linkage")
set(SDRPP_DEP_ROOT_PACKAGES "" CACHE STRING "Comma/space/semicolon-separated top-level dependency recipes required by the parent SDRIAK build")
set(SDRPP_TRACE_DEP_ARTIFACTS "OFF" CACHE STRING "Print validated dependency artifacts to the console (ON/all, or comma/semicolon-separated dependency names)")
option(SDRPP_DEP_POLICY_STRICT "Require explicit policy metadata for every dependency" OFF)

option(BUILD_SHARED_LIBS "Build shared libraries instead of static" ON)
set(${PROJECT_NAME}_PACKAGE_EXCLUDES "" CACHE STRING "Exclude packages matching this regex pattern")

include(${PROJECT_SOURCE_DIR}/cmake/DepPolicy.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/DepClassification.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/AddCMakeProject.cmake)

# Discover all +<Name> folders containing a <Name>.cmake recipe.
macro(list_projects result curdir)
    file(GLOB children RELATIVE ${curdir} ${curdir}/*)
    set(_dirlist "")
    foreach (child ${children})
        if (IS_DIRECTORY ${curdir}/${child})
            string(REGEX MATCH "^\\+([a-zA-Z0-9_-]+)" _is_pkg ${child})
            if (_is_pkg AND EXISTS ${curdir}/${child}/${CMAKE_MATCH_1}.cmake)
                list(APPEND _dirlist ${CMAKE_MATCH_1})
            endif ()
        endif ()
    endforeach ()
    set(${result} ${_dirlist})
endmacro()

# Print first-configure-only status chatter. Warnings and fatal errors must use
# message() directly so DEP_MESSAGES_WRITTEN can never suppress error diagnostics.
function(dep_status msg)
    if (NOT DEP_MESSAGES_WRITTEN)
        message(STATUS "${msg}")
    endif ()
endfunction()

function(sdrpp_dep_is_excluded pkg out_var)
    set(_excluded FALSE)
    if (${PROJECT_NAME}_PACKAGE_EXCLUDES)
        if (${pkg} MATCHES "${${PROJECT_NAME}_PACKAGE_EXCLUDES}")
            set(_excluded TRUE)
        endif ()
    endif ()
    set(${out_var} ${_excluded} PARENT_SCOPE)
endfunction()

macro(sdrpp_include_dep_recipe pkg)
    if (NOT ${pkg} IN_LIST FOUND_PACKAGES)
        message(FATAL_ERROR "Requested dependency '${pkg}' does not exist under deps/.")
    endif ()
    if (NOT ${pkg} IN_LIST SUPPORTED_PACKAGES)
        include(+${pkg}/${pkg}.cmake)
        list(APPEND SUPPORTED_PACKAGES ${pkg})
    endif ()
endmacro()

# Architecture sniff (Windows MSVC).
if (MSVC)
    if ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
        dep_status("Detected 64-bit MSVC compiler")
    elseif ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
        dep_status("Detected 32-bit MSVC compiler")
    else ()
        message(FATAL_ERROR "Unable to detect target architecture")
    endif ()
else ()
    set(DEP_CMAKE_OPTS "-DCMAKE_POSITION_INDEPENDENT_CODE=ON")
endif ()

# Many SDR libraries ship their own FindLIBUSB.cmake or call pkg_check_modules
# for libusb-1.0, with no fallback to find_package(libusb CONFIG). When we
# build libusb ourselves (bundled policy), pre-feed the variables they expect
# against our deps install prefix so every sub-build can locate it without
# needing pkg-config on the host. When libusb is system-provided (distro
# profile), skip the override — destdir doesn't contain libusb, and the host
# has pkg-config available to discover the real system paths.
sdrpp_dep_is_system(libusb _libusb_is_system)
if (NOT _libusb_is_system)
    set(_libusb_inc "${SDRPP_DEPS_INSTALL_PREFIX}/include/libusb-1.0")
    if (WIN32)
        set(_libusb_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/usb-1.0.lib")
    elseif (APPLE)
        set(_libusb_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/libusb-1.0.dylib")
    else ()
        set(_libusb_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/libusb-1.0.so")
    endif ()
    list(APPEND DEP_CMAKE_OPTS
        # singular forms (FindLIBUSB.cmake convention used by libairspy/airspyhf/
        # hackrf/perseus and most SDR forks)
        "-DLIBUSB_INCLUDE_DIR=${_libusb_inc}"
        "-DLIBUSB_LIBRARY=${_libusb_lib}"
        # plural forms (libiio convention via pkg_check_modules result vars)
        "-DLIBUSB_INCLUDE_DIRS=${_libusb_inc}"
        "-DLIBUSB_LIBRARIES=${_libusb_lib}"
        "-DLIBUSB_FOUND=TRUE"
        "-DLIBUSB_VERSION=1.0.27"
    )
endif ()

# libiio v0.24 uses CMake's FindLibXml2 module and does not reliably pick up
# the libxml2 Config package from the cross-built prefix. Feed the legacy
# module variables directly, matching the libusb override style above — but
# only when libxml2 is bundled. On distro builds libxml2 is system-provided,
# so let dep recipes discover it through find_package / pkg-config naturally.
sdrpp_dep_is_system(libxml2 _libxml2_is_system)
if (NOT _libxml2_is_system)
    set(_libxml2_inc "${SDRPP_DEPS_INSTALL_PREFIX}/include/libxml2")
    if (WIN32)
        set(_libxml2_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/libxml2.lib")
    elseif (APPLE)
        set(_libxml2_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/libxml2.dylib")
    else ()
        set(_libxml2_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/libxml2.so")
    endif ()
    list(APPEND DEP_CMAKE_OPTS
        "-DLIBXML2_INCLUDE_DIR=${_libxml2_inc}"
        "-DLIBXML2_INCLUDE_DIRS=${_libxml2_inc}"
        "-DLIBXML2_LIBRARY=${_libxml2_lib}"
        "-DLIBXML2_LIBRARIES=${_libxml2_lib}"
        "-DLIBXML2_FOUND=TRUE"
    )
endif ()

# Android has system pthreads in Bionic. When cross-compiling, FindThreads
# cannot run its probe binary, so pre-feed the compiler flag used by the
# Android toolchain instead of building or linking a separate pthreads library.
if (ANDROID)
    list(APPEND DEP_CMAKE_OPTS
        "-DTHREADS_PTHREAD_ARG=-pthread"
    )
endif ()

# Windows ARM64 cross-compile from an x64 host: CMake's compiler probe
# correctly identifies the target arch and sets CMAKE_C_COMPILER_ARCHITECTURE_ID,
# but CMAKE_SYSTEM_PROCESSOR stays at the host value (AMD64) because no
# toolchain file is in play. That breaks dep CMakeLists that gate
# x86/x64/arm/arm64 logic on CMAKE_SYSTEM_PROCESSOR (volk's SSE/NEON
# dispatch, others).
#
# Forwarding -DCMAKE_SYSTEM_PROCESSOR alone isn't enough: CMake's
# CMakeDetermineSystem.cmake only honors that variable when CMAKE_SYSTEM_NAME
# (or a toolchain file) is set — otherwise project() overwrites it with the
# host value. So we also force CMAKE_SYSTEM_NAME=Windows for the cross case,
# which puts CMake into "explicit system" mode and keeps the user-supplied
# CMAKE_SYSTEM_PROCESSOR sticky. Side effect: CMAKE_CROSSCOMPILING becomes
# TRUE, which our codec2 recipe already handles via its GENERATE_CODEBOOK
# preflight path.
#
# Gated on host != ARM64 so a native ARM64-host build doesn't unnecessarily
# enter cross-compile mode.
if (WIN32 AND CMAKE_C_COMPILER_ARCHITECTURE_ID MATCHES "^ARM64"
        AND NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^([Aa][Rr][Mm]64|[Aa][Aa][Rr][Cc][Hh]64)")
    list(APPEND DEP_CMAKE_OPTS
        "-DCMAKE_SYSTEM_NAME:STRING=Windows"
        "-DCMAKE_SYSTEM_PROCESSOR:STRING=ARM64"
    )
endif ()

# pthreads4w (Windows only) — Windows sibling forks read pthreads paths
# through two incompatible bundled find modules: the libairspy / libairspyhf
# / libhackrf / libhydrasdr / libmirisdr forks use CMake's FindThreads style
# (THREADS_PTHREADS_*), while libbladeRF uses its own FindLibPThreadsWin32
# style (LIBPTHREADSWIN32_* + PTHREAD_LIBRARY / PTHREAD_DLL, plus a COPYING.LIB
# sentinel under LIBPTHREADSWIN32_PATH that +pthreads/pthreads.cmake stubs).
# Pre-feed both naming schemes here against the layout that pthreads.cmake
# installs; the args are harmless cache entries to deps that don't read them.
if (WIN32)
    set(_pthreads4w_inc "${SDRPP_DEPS_INSTALL_PREFIX}/include")
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_pthreads4w_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/pthreadVC3d.lib")
        set(_pthreads4w_dll "${SDRPP_DEPS_INSTALL_PREFIX}/bin/pthreadVC3d.dll")
    else ()
        set(_pthreads4w_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/pthreadVC3.lib")
        set(_pthreads4w_dll "${SDRPP_DEPS_INSTALL_PREFIX}/bin/pthreadVC3.dll")
    endif ()
    list(APPEND DEP_CMAKE_OPTS
        # FindThreads style (libairspy, libairspyhf, libhackrf, libhydrasdr, libmirisdr)
        "-DTHREADS_PTHREADS_INCLUDE_DIR=${_pthreads4w_inc}"
        "-DTHREADS_PTHREADS_WIN32_LIBRARY=${_pthreads4w_lib}"
        # FindLibPThreadsWin32 style (libbladeRF)
        "-DLIBPTHREADSWIN32_PATH=${SDRPP_DEPS_COMPAT_DIR}/pthreads-win32"
        "-DLIBPTHREADSWIN32_FOUND=TRUE"
        "-DLIBPTHREADSWIN32_HEADER_FILE=${_pthreads4w_inc}/pthread.h"
        "-DPTHREAD_LIBRARY=${_pthreads4w_lib}"
        "-DPTHREAD_DLL=${_pthreads4w_dll}"
    )
endif ()

# FFTW (single precision) — used by libhackrf's hackrf-tools and others.
# Bundled FindFFTW.cmake modules in the SDR forks expect FFTW_LIBRARIES /
# FFTW_INCLUDES / FFTW_INCLUDE_DIRS / FFTW_FOUND. Pre-feed them.
set(_fftw_inc "${SDRPP_DEPS_INSTALL_PREFIX}/include")
if (WIN32)
    set(_fftw_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/fftw3f.lib")
elseif (APPLE)
    set(_fftw_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/libfftw3f.dylib")
else ()
    set(_fftw_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/libfftw3f.so")
endif ()
list(APPEND DEP_CMAKE_OPTS
    "-DFFTW_LIBRARIES=${_fftw_lib}"
    "-DFFTW_INCLUDES=${_fftw_inc}"
    "-DFFTW_INCLUDE_DIRS=${_fftw_inc}"
    "-DFFTW_INCLUDE_DIR=${_fftw_inc}"
    "-DFFTW_FOUND=TRUE"
)

# libad9361 depends on libiio and uses LIBIIO_INCLUDEDIR / LIBIIO_LIBRARIES
# (FindLIBIIO.cmake style). Pre-feed the paths to the libiio we built so
# every sub-build that receives DEP_CMAKE_OPTS (including libad9361) can
# locate it without pkg-config.
set(_libiio_inc "${SDRPP_DEPS_INSTALL_PREFIX}/include")
if (WIN32)
    set(_libiio_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/iio.lib")
elseif (APPLE)
    set(_libiio_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/libiio.dylib")
else ()
    set(_libiio_lib "${SDRPP_DEPS_INSTALL_PREFIX}/lib/libiio.so")
endif ()
list(APPEND DEP_CMAKE_OPTS
    "-DLIBIIO_INCLUDEDIR=${_libiio_inc}"
    "-DLIBIIO_LIBRARIES=${_libiio_lib}"
    "-DLIBIIO_FOUND=TRUE"
)

list_projects(FOUND_PACKAGES ${CMAKE_CURRENT_LIST_DIR})
dep_status("Found dependency recipes: ${FOUND_PACKAGES}")
dep_status("Dependency policy profile: ${SDRPP_DEP_PROFILE}")
sdrpp_normalize_dep_list(_requested_root_packages "${SDRPP_DEP_ROOT_PACKAGES}")
if (_requested_root_packages)
    dep_status("Requested dependency roots: ${_requested_root_packages}")
endif ()

# Packages provided by the host platform (skip building, find via find_package).
# Populated as we extend to Linux/macOS; on Windows everything is built from source.
set(${PROJECT_NAME}_PLATFORM_PACKAGES "" CACHE STRING "Packages provided by the platform")
set(SYSTEM_PROVIDED_PACKAGES OpenGL)
list(APPEND SYSTEM_PROVIDED_PACKAGES ${${PROJECT_NAME}_PLATFORM_PACKAGES})
list(REMOVE_DUPLICATES SYSTEM_PROVIDED_PACKAGES)

include(CMakeDependentOption)
option(${PROJECT_NAME}_SELECT_ALL "Build all dependencies" ON)

find_package(Git REQUIRED)
set(PATCH_CMD ${GIT_EXECUTABLE} apply --verbose --ignore-space-change --whitespace=fix)

set(DEPS_TO_BUILD "")
set(_build_list "")
set(_build_list_toplevel "")
set(_checked_list "")

function(check_system_package pkg checked_list)
    if (NOT ${pkg} IN_LIST ${checked_list})
        find_package(${pkg})
        if (NOT ${pkg}_FOUND)
            message(WARNING "No ${pkg} found in system although marked as system-provided.")
        endif ()
        list(APPEND ${checked_list} ${pkg})
        set(${checked_list} ${${checked_list}} PARENT_SCOPE)
    endif ()
endfunction()

set(SUPPORTED_PACKAGES "")
set(_selected_root_packages "")
foreach (pkg ${FOUND_PACKAGES})
    cmake_dependent_option(${PROJECT_NAME}_SELECT_${pkg}
        "Select ${pkg} for build" OFF
        "NOT ${PROJECT_NAME}_SELECT_ALL" OFF)

    sdrpp_dep_is_excluded(${pkg} _excluded)
    if (_requested_root_packages)
        if (${pkg} IN_LIST _requested_root_packages)
            if (_excluded)
                message(FATAL_ERROR "Requested dependency root '${pkg}' is excluded by ${PROJECT_NAME}_PACKAGE_EXCLUDES.")
            endif ()
            list(APPEND _selected_root_packages ${pkg})
        endif ()
    elseif (NOT _excluded AND (${PROJECT_NAME}_SELECT_ALL OR ${PROJECT_NAME}_SELECT_${pkg}))
        list(APPEND _selected_root_packages ${pkg})
    endif ()
endforeach ()

if (_requested_root_packages)
    set(_missing_requested_roots ${_requested_root_packages})
    list(REMOVE_ITEM _missing_requested_roots ${_selected_root_packages})
    if (_missing_requested_roots)
        message(FATAL_ERROR "Requested dependency roots were not found or were filtered out: ${_missing_requested_roots}")
    endif ()
endif ()

set(_pending_packages ${_selected_root_packages})
while (_pending_packages)
    list(POP_FRONT _pending_packages pkg)
    if (${pkg} IN_LIST SUPPORTED_PACKAGES)
        continue()
    endif ()

    sdrpp_dep_is_excluded(${pkg} _excluded)
    if (_excluded)
        message(FATAL_ERROR "Dependency '${pkg}' is required but excluded by ${PROJECT_NAME}_PACKAGE_EXCLUDES.")
    endif ()

    sdrpp_include_dep_recipe(${pkg})
    foreach (deppkg ${DEP_${pkg}_DEPENDS})
        if (${deppkg} IN_LIST FOUND_PACKAGES)
            list(APPEND _pending_packages ${deppkg})
        endif ()
    endforeach ()
endwhile ()

foreach (pkg ${SUPPORTED_PACKAGES})
    sdrpp_resolve_dep_policy(${pkg} _policy)
    dep_status("  policy ${pkg}: source=${_policy_SOURCE}, linkage=${_policy_LINKAGE}, usage=${_policy_USAGE}")

    if (_policy_IS_SYSTEM)
        check_system_package(${pkg} _checked_list)
    elseif (TARGET dep_${pkg})
        get_target_property(_excl_all dep_${pkg} EXCLUDE_FROM_ALL)
        if (NOT _excl_all)
            list(APPEND DEPS_TO_BUILD ${pkg})
        endif ()
    endif ()
endforeach ()

# Dependency graph between packages (DEP_<Name>_DEPENDS = list of other recipe names).
foreach (pkg ${SUPPORTED_PACKAGES})
    if (${pkg} IN_LIST DEPS_TO_BUILD)
        list(APPEND _build_list dep_${pkg})
        list(APPEND _build_list_toplevel dep_${pkg})
    endif ()
    foreach (deppkg ${DEP_${pkg}_DEPENDS})
        sdrpp_dep_is_system(${deppkg} _dep_is_system)
        if (_dep_is_system)
            check_system_package(${deppkg} _checked_list)
        elseif (TARGET dep_${deppkg})
            dep_status("  ${pkg} depends on ${deppkg}")
            add_dependencies(dep_${pkg} dep_${deppkg})
            list(APPEND _build_list dep_${deppkg})
        endif ()
    endforeach ()
endforeach ()

list(REMOVE_DUPLICATES _build_list)
dep_status("Building dep targets (${CMAKE_BUILD_TYPE}): ${_build_list}")
add_custom_target(deps ALL DEPENDS ${_build_list_toplevel})

set(DEP_MESSAGES_WRITTEN ON CACHE BOOL "")

install(CODE "message(STATUS \"SDRIAK dependencies built into ${SDRPP_DEPS_INSTALL_PREFIX}\")")
