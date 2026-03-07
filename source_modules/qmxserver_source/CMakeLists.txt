cmake_minimum_required(VERSION 3.13)
project(qmxserver_source)

add_subdirectory(src/enet)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(qmxserver_source PRIVATE "src/" "src/enet/include/")
target_link_libraries(qmxserver_source PRIVATE enet)

if (WIN32)
    target_link_libraries(qmxserver_source PRIVATE winmm)
endif()
