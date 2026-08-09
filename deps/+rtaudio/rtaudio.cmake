#
# RtAudio — used by OPT_BUILD_AUDIO_SINK / OPT_BUILD_AUDIO_SOURCE and the
# non-Android path of OPT_BUILD_QMX_SOURCE. Android uses Oboe.
#
# Pinned to the same commit the existing Windows CI uses (build_all.yml line
# 164/297) — known to work with SDR++. Bump only after verifying, and re-check
# patches/ against the new pin.
#
sdrpp_dep_get_linkage_option_bools(rtaudio _rtaudio_build_shared _rtaudio_build_static)

add_cmake_project(rtaudio
    GIT_REPOSITORY https://github.com/thestk/rtaudio
    GIT_TAG        2f2fca4502d506abc50f6d4473b2836d24cfb1e3
    GIT_SHALLOW    OFF
    PATCH_COMMAND  ${CMAKE_COMMAND}
                       -DSRC=<SOURCE_DIR>
                       -P ${CMAKE_CURRENT_LIST_DIR}/patch_rtaudio.cmake
    CMAKE_ARGS
        -DRTAUDIO_BUILD_TESTING=OFF
        -DRTAUDIO_BUILD_STATIC_LIBS=${_rtaudio_build_static}
        -DRTAUDIO_BUILD_SHARED_LIBS=${_rtaudio_build_shared}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
)

# Upstream rtaudio at this pin doesn't install Config.cmake into a
# layout we can probe by recipe name — file-existence probes only.
sdrpp_validate_dep(rtaudio
    TARGET         RtAudio::rtaudio
    LIB_NAMES      rtaudio rtaudio_static
    DLL_NAMES      rtaudio.dll
    HEADER         RtAudio.h
    INCLUDE_SUBDIR rtaudio)
