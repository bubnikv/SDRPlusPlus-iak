cmake_minimum_required(VERSION 3.13)
project(qmx_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

add_subdirectory("libqmx")
target_link_libraries(qmx_source PRIVATE qmx)
