#
# SDRplay API — closed-source binary blob. Distribution differs per OS:
#   * Windows: SDRplay's official Inno Setup .exe installer, extracted with
#     innoextract.
#   * Linux: SDRplay's official Makeself .run installer from sdrplay.com,
#     extracted via 7z (avoids running the EULA-prompting shell wrapper).
#   * macOS: SDRplay's official universal .pkg installer, extracted with
#     pkgutil.
#
# All three paths land the artifacts under the shared deps prefix so the
# Config.cmake emitted at the bottom of this file finds them uniformly.
#

include(ExternalProject)

sdrpp_dep_builds_shared(sdrplay _sdrplay_builds_shared)
if (NOT _sdrplay_builds_shared)
    message(FATAL_ERROR "sdrplay is distributed as a shared binary only; remove it from SDRPP_DEP_FORCE_STATIC.")
endif ()

set(_prefix ${SDRPP_DEPS_INSTALL_PREFIX})

if (WIN32)
    # SDRplay's Windows API ships only x86/x64 user-mode binaries and matching
    # x86/x64 kernel drivers. ARM64 Windows cannot load those drivers, so the
    # device never enumerates. Skip on any ARM64 target — the check runs before
    # the x64 branch so CMAKE_SIZEOF_VOID_P=8 on ARM64 doesn't mis-route us
    # into installing an unusable x64 DLL.
    if (CMAKE_GENERATOR_PLATFORM MATCHES "^ARM64")
        message(WARNING "SDRplay: no ARM64 Windows kernel driver available, skipping")
        return()
    elseif (CMAKE_GENERATOR_PLATFORM STREQUAL "x64" OR CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_arch_dir x64)
    elseif (CMAKE_GENERATOR_PLATFORM STREQUAL "Win32" OR CMAKE_GENERATOR_PLATFORM STREQUAL "")
        set(_arch_dir x86)
    endif ()

    # SDRplay's Windows API is shipped as an Inno Setup installer .exe. CMake
    # can't unpack Inno's compressed data segment natively (7z peels only the
    # outer PE wrapper and stops at the Inno container), so bootstrap the
    # standard offline extractor — innoextract — from its GitHub release.
    # Pinned by SHA256, cached in the deps build tree across reconfigures.
    set(_innoextract_version 1.9)
    set(_innoextract_zip innoextract-${_innoextract_version}-windows.zip)
    set(_innoextract_url
        https://github.com/dscharrer/innoextract/releases/download/${_innoextract_version}/${_innoextract_zip})
    set(_innoextract_dir ${CMAKE_CURRENT_BINARY_DIR}/tools/innoextract-${_innoextract_version})
    set(_innoextract_exe ${_innoextract_dir}/innoextract.exe)
    if (NOT EXISTS "${_innoextract_exe}")
        file(MAKE_DIRECTORY "${_innoextract_dir}")
        message(STATUS "SDRplay: bootstrapping innoextract ${_innoextract_version} for installer extraction")
        file(DOWNLOAD
            "${_innoextract_url}"
            "${_innoextract_dir}/${_innoextract_zip}"
            EXPECTED_HASH SHA256=6989342c9b026a00a72a38f23b62a8e6a22cc5de69805cf47d68ac2fec993065
            SHOW_PROGRESS
            STATUS _innoextract_dl_status)
        list(GET _innoextract_dl_status 0 _innoextract_dl_code)
        if (NOT _innoextract_dl_code EQUAL 0)
            message(FATAL_ERROR "SDRplay: failed to download innoextract from ${_innoextract_url}: ${_innoextract_dl_status}")
        endif ()
        file(ARCHIVE_EXTRACT
            INPUT "${_innoextract_dir}/${_innoextract_zip}"
            DESTINATION "${_innoextract_dir}")
        if (NOT EXISTS "${_innoextract_exe}")
            message(FATAL_ERROR "SDRplay: innoextract bootstrap finished but ${_innoextract_exe} is missing — release layout changed?")
        endif ()
    endif ()

    # Naming convention on sdrplay.com is major.minor only for Windows
    # (Linux/.run and macOS/.pkg carry a patch revision; Windows doesn't).
    set(_sdrplay_pkg_version_win 3.15)
    set(_sdrplay_pkg_basename_win SDRplay_RSP_API-Windows-${_sdrplay_pkg_version_win}.exe)
    set(_sdrplay_pkg_url_win https://sdrplay.com/download/hardware-api-windows/?wpdmdl=1905)

    set(_sdrplay_scratch ${CMAKE_CURRENT_BINARY_DIR}/sources/sdrplay/extracted)

    ExternalProject_Add(dep_sdrplay
        URL                 ${_sdrplay_pkg_url_win}
        URL_HASH            SHA256=8ad5c36f1ca26cf7a61010c3f3c80dae69d4468ef5e59f7a0d42fb135a1c7326
        DOWNLOAD_NAME       ${_sdrplay_pkg_basename_win}
        DOWNLOAD_DIR        ${${PROJECT_NAME}_DEP_DOWNLOAD_DIR}/sdrplay
        DOWNLOAD_NO_EXTRACT TRUE
        SOURCE_DIR          ${CMAKE_CURRENT_BINARY_DIR}/sources/sdrplay
        BINARY_DIR          ${CMAKE_CURRENT_BINARY_DIR}/builds/sdrplay
        CONFIGURE_COMMAND   ""
        BUILD_COMMAND       ""
        INSTALL_COMMAND
                ${CMAKE_COMMAND} -E rm -rf ${_sdrplay_scratch}
            COMMAND
                ${_innoextract_exe} --silent --exclude-temp
                    --output-dir ${_sdrplay_scratch}
                    ${${PROJECT_NAME}_DEP_DOWNLOAD_DIR}/sdrplay/${_sdrplay_pkg_basename_win}
            COMMAND
                ${CMAKE_COMMAND}
                    -DSCRATCH=${_sdrplay_scratch}
                    -DARCH=${_arch_dir}
                    -DPREFIX=${_prefix}
                    -P ${CMAKE_CURRENT_LIST_DIR}/install_windows.cmake
        USES_TERMINAL_INSTALL ${SDRPP_SERIALIZE_CMAKE_INVOCATIONS}
    )
elseif (UNIX AND NOT APPLE)
    # SDRplay's Linux installer is a Makeself .run — a shell wrapper around a
    # tar.gz payload. Running it would prompt for an EULA, so extract with 7z
    # instead (same two-pass approach the pre-rework do_build.sh used: pass 1
    # peels the shell wrapper, pass 2 unpacks the inner tar).
    set(_sdrplay_run_version 3.15.2)
    set(_sdrplay_run_basename SDRplay_RSP_API-Linux-${_sdrplay_run_version})
    set(_sdrplay_run_url https://sdrplay.com/download/hardware-api-linux/?wpdmdl=1906)

    # Inner archive's per-arch directory names mirror `uname -m` on the targets
    # SDRplay supports. CMAKE_SYSTEM_PROCESSOR can come through as either
    # `aarch64`/`arm64` and `x86_64`/`amd64` depending on the toolchain, so
    # normalize both spellings.
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        set(_sdrplay_arch_dir x86_64)
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(_sdrplay_arch_dir aarch64)
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^armv7")
        set(_sdrplay_arch_dir armv7l)
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(i686|i386)$")
        set(_sdrplay_arch_dir i686)
    else ()
        message(FATAL_ERROR "SDRplay: unsupported Linux host architecture '${CMAKE_SYSTEM_PROCESSOR}'")
    endif ()

    find_program(_sdrplay_7z NAMES 7z 7zz 7za)
    if (NOT _sdrplay_7z)
        message(FATAL_ERROR
            "SDRplay (Linux): 7z not found on PATH. "
            "Install p7zip-full (Debian/Ubuntu) or equivalent to extract the SDRplay installer.")
    endif ()

    # The .so inside the installer is versioned major.minor (e.g. .so.3.15);
    # derive it from the installer version so a future SDRplay bump only needs
    # to change _sdrplay_run_version above. The SONAME embedded in the .so is
    # major-only (e.g. libsdrplay_api.so.3) — plugins linked against the API get
    # that as their NEEDED entry, so we must publish a matching symlink for
    # linuxdeploy/ldd to resolve at AppImage bundle time.
    string(REGEX MATCH "^([0-9]+\\.[0-9]+)" _sdrplay_so_mm "${_sdrplay_run_version}")
    string(REGEX MATCH "^([0-9]+)" _sdrplay_so_major "${_sdrplay_run_version}")
    set(_sdrplay_so_versioned libsdrplay_api.so.${_sdrplay_so_mm})
    set(_sdrplay_so_soname libsdrplay_api.so.${_sdrplay_so_major})

    set(_sdrplay_scratch ${CMAKE_CURRENT_BINARY_DIR}/sources/sdrplay/extracted)

    ExternalProject_Add(dep_sdrplay
        URL                 ${_sdrplay_run_url}
        URL_HASH            SHA256=3a97ca764263bbe76fb0f2220e6408942357e8864c19e1408a6d6987af382fe3
        DOWNLOAD_NAME       ${_sdrplay_run_basename}.run
        DOWNLOAD_DIR        ${${PROJECT_NAME}_DEP_DOWNLOAD_DIR}/sdrplay
        DOWNLOAD_NO_EXTRACT TRUE
        SOURCE_DIR          ${CMAKE_CURRENT_BINARY_DIR}/sources/sdrplay
        BINARY_DIR          ${CMAKE_CURRENT_BINARY_DIR}/builds/sdrplay
        CONFIGURE_COMMAND   ""
        BUILD_COMMAND       ""
        INSTALL_COMMAND
                ${CMAKE_COMMAND} -E rm -rf ${_sdrplay_scratch}
            COMMAND
                ${CMAKE_COMMAND} -E make_directory ${_sdrplay_scratch}
            COMMAND
                ${_sdrplay_7z} x -y -o${_sdrplay_scratch}
                    ${${PROJECT_NAME}_DEP_DOWNLOAD_DIR}/sdrplay/${_sdrplay_run_basename}.run
            COMMAND
                ${_sdrplay_7z} x -y -o${_sdrplay_scratch}
                    ${_sdrplay_scratch}/${_sdrplay_run_basename}
            COMMAND
                ${CMAKE_COMMAND}
                    -DSCRATCH=${_sdrplay_scratch}
                    -DARCH=${_sdrplay_arch_dir}
                    -DPREFIX=${_prefix}
                    -DSO_VERSIONED=${_sdrplay_so_versioned}
                    -DSO_SONAME=${_sdrplay_so_soname}
                    -P ${CMAKE_CURRENT_LIST_DIR}/install_sdrplay.cmake
        USES_TERMINAL_INSTALL ${SDRPP_SERIALIZE_CMAKE_INVOCATIONS}
    )
else ()
    # macOS: SDRplay ships SDRplayAPI-macos-installer-universal-*.pkg — a
    # universal binary (x86_64 + arm64), so the same .pkg covers both Intel
    # and Apple Silicon. Extract offline with `pkgutil --expand-full` (no
    # sudo, no installation into /usr/local) and plant the library + headers
    # under the deps prefix, mirroring the Linux flow above.
    #
    # SDRplay's macOS .pkg lags the Linux .run version slightly; track it
    # independently. SDRplay uses the .so extension on macOS instead of
    # .dylib (idiosyncratic but consistent with their tooling).
    set(_sdrplay_pkg_version 3.15.1)
    set(_sdrplay_pkg_basename SDRplayAPI-macos-installer-universal-${_sdrplay_pkg_version}.pkg)
    set(_sdrplay_pkg_url https://sdrplay.com/download/hardware-api-macos/?wpdmdl=1907)

    find_program(_sdrplay_pkgutil pkgutil)
    if (NOT _sdrplay_pkgutil)
        message(FATAL_ERROR
            "SDRplay (macOS): pkgutil not found on PATH (this is a stock macOS "
            "system tool — check the build environment).")
    endif ()

    string(REGEX MATCH "^([0-9]+\\.[0-9]+)" _sdrplay_so_mm "${_sdrplay_pkg_version}")
    string(REGEX MATCH "^([0-9]+)" _sdrplay_so_major "${_sdrplay_pkg_version}")
    set(_sdrplay_so_versioned libsdrplay_api.so.${_sdrplay_so_mm})
    set(_sdrplay_so_soname libsdrplay_api.so.${_sdrplay_so_major})

    set(_sdrplay_scratch ${CMAKE_CURRENT_BINARY_DIR}/sources/sdrplay/extracted)

    ExternalProject_Add(dep_sdrplay
        URL                 ${_sdrplay_pkg_url}
        URL_HASH            SHA256=5d148ceda1fae775d2d2df5b3fcf46dee27b6222e2535c4c063c2f2ea2f7acc3
        DOWNLOAD_NAME       ${_sdrplay_pkg_basename}
        DOWNLOAD_NO_EXTRACT TRUE
        DOWNLOAD_DIR        ${${PROJECT_NAME}_DEP_DOWNLOAD_DIR}/sdrplay
        SOURCE_DIR          ${CMAKE_CURRENT_BINARY_DIR}/sources/sdrplay
        BINARY_DIR          ${CMAKE_CURRENT_BINARY_DIR}/builds/sdrplay
        CONFIGURE_COMMAND   ""
        BUILD_COMMAND       ""
        INSTALL_COMMAND
                ${CMAKE_COMMAND} -E rm -rf ${_sdrplay_scratch}
            COMMAND
                ${_sdrplay_pkgutil} --expand-full
                    ${${PROJECT_NAME}_DEP_DOWNLOAD_DIR}/sdrplay/${_sdrplay_pkg_basename}
                    ${_sdrplay_scratch}
            COMMAND
                ${CMAKE_COMMAND}
                    -DSCRATCH=${_sdrplay_scratch}
                    -DPREFIX=${_prefix}
                    -DSO_VERSIONED=${_sdrplay_so_versioned}
                    -DSO_SONAME=${_sdrplay_so_soname}
                    -P ${CMAKE_CURRENT_LIST_DIR}/install_sdrplay.cmake
        USES_TERMINAL_INSTALL ${SDRPP_SERIALIZE_CMAKE_INVOCATIONS}
    )
endif ()

sdrpp_emit_imported_config(sdrplay
    TARGET      sdrplay::sdrplay_api
    LIB_NAMES   sdrplay_api
    DLL_NAMES   sdrplay_api.dll
    HEADER      sdrplay_api.h
    INCLUDE_SUBDIR SDRplay
)
