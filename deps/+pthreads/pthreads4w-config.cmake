#
# pthreads4w (PThreads4W v3) — CMake package config installed by the
# SDRIAK deps superbuild.
#
# Defines the imported target pthreads4w::pthreadVC3 (SHARED) wired to the
# artifacts that pthreads.cmake installs:
#   <prefix>/include/pthread.h          (and friends)
#   <prefix>/lib/pthreadVC3[d].lib      (import library)
#   <prefix>/bin/pthreadVC3[d].dll      (runtime DLL)
#
# Located by the standard find_package search at
#   <prefix>/lib/cmake/pthreads4w/pthreads4w-config.cmake
#
# Consumers invoke find_package(pthreads4w CONFIG) and link against
# pthreads4w::pthreadVC3. The legacy result variables (INCLUDE_DIR /
# PTHREADVC3_LIBRARY / PTHREADVC3_DLL) are also set, for sibling deps
# whose bundled Find modules read them directly.
#

get_filename_component(_pthreads4w_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    set(_pthreads4w_no_cache NO_CACHE)
else ()
    set(_pthreads4w_no_cache "")
endif ()

# Prefer the d-postfixed (debug) artifact when consumed by a Debug build,
# the unsuffixed one otherwise. find_*() returns the first existing match,
# so the alternate ordering still resolves a single-config install.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_pthreads4w_lib_names pthreadVC3d pthreadVC3)
    set(_pthreads4w_dll_names pthreadVC3d.dll pthreadVC3.dll)
else ()
    set(_pthreads4w_lib_names pthreadVC3 pthreadVC3d)
    set(_pthreads4w_dll_names pthreadVC3.dll pthreadVC3d.dll)
endif ()

find_path(pthreads4w_INCLUDE_DIR
    NAMES pthread.h
    HINTS "${_pthreads4w_root}/include"
    NO_DEFAULT_PATH ${_pthreads4w_no_cache})

find_library(pthreads4w_PTHREADVC3_LIBRARY
    NAMES ${_pthreads4w_lib_names}
    HINTS "${_pthreads4w_root}/lib"
    NO_DEFAULT_PATH ${_pthreads4w_no_cache})

find_file(pthreads4w_PTHREADVC3_DLL
    NAMES ${_pthreads4w_dll_names}
    HINTS "${_pthreads4w_root}/bin"
    NO_DEFAULT_PATH ${_pthreads4w_no_cache})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(pthreads4w
    REQUIRED_VARS
        pthreads4w_INCLUDE_DIR
        pthreads4w_PTHREADVC3_LIBRARY
        pthreads4w_PTHREADVC3_DLL)

if (pthreads4w_FOUND AND NOT TARGET pthreads4w::pthreadVC3)
    add_library(pthreads4w::pthreadVC3 SHARED IMPORTED)
    set_target_properties(pthreads4w::pthreadVC3 PROPERTIES
        IMPORTED_LOCATION             "${pthreads4w_PTHREADVC3_DLL}"
        IMPORTED_IMPLIB               "${pthreads4w_PTHREADVC3_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${pthreads4w_INCLUDE_DIR}")
endif ()
