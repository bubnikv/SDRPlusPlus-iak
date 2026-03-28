cmake_minimum_required(VERSION 3.13)
project(libqmx)

file(GLOB_RECURSE LIBQMX_SRC "src/*.cpp")

add_library(qmx STATIC ${LIBQMX_SRC})
set_target_properties(qmx PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(qmx PUBLIC cxx_std_17)
target_include_directories(qmx PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

if (MSVC)
    target_link_libraries(qmx PRIVATE ole32 avrt winmm)
elseif (ANDROID)
    target_include_directories(qmx PRIVATE "${SDR_KIT_ROOT}/${ANDROID_ABI}/include")
    target_link_libraries(qmx PRIVATE
        ${SDR_KIT_ROOT}/${ANDROID_ABI}/lib/libusb1.0.so
    )
elseif (UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA REQUIRED alsa)

    target_include_directories(qmx PRIVATE ${ALSA_INCLUDE_DIRS})
    target_link_directories(qmx PRIVATE ${ALSA_LIBRARY_DIRS})
    target_link_libraries(qmx PRIVATE ${ALSA_LIBRARIES})
endif ()
