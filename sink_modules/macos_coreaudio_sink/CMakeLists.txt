cmake_minimum_required(VERSION 3.13)
project(macos_coreaudio_sink)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_link_libraries(macos_coreaudio_sink PRIVATE
    "-framework CoreAudio"
    "-framework AudioToolbox"
    "-framework AudioUnit"
    "-framework CoreFoundation"
)
