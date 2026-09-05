cmake_minimum_required(VERSION 3.16...3.24)
project(sdriak_server_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_include_directories(sdriak_server_source PRIVATE "src/")
