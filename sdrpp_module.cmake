# Get needed values depending on if this is in-tree or out-of-tree
if (NOT SDRPP_CORE_ROOT)
    set(SDRPP_CORE_ROOT "@SDRPP_CORE_ROOT@")
endif ()
if (NOT SDRPP_MODULE_RUNTIME_OUTPUT_DIRECTORY)
    set(SDRPP_MODULE_RUNTIME_OUTPUT_DIRECTORY "@SDRPP_MODULE_RUNTIME_OUTPUT_DIRECTORY@")
    if (SDRPP_MODULE_RUNTIME_OUTPUT_DIRECTORY MATCHES "^@")
        set(SDRPP_MODULE_RUNTIME_OUTPUT_DIRECTORY "")
    endif ()
endif ()

# Created shared lib and link to core
add_library(${PROJECT_NAME} SHARED ${SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE sdrpp_core)
target_include_directories(${PROJECT_NAME} PRIVATE
    "${SDRPP_CORE_ROOT}"
    "${SDRPP_CORE_ROOT}/imgui")
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
if (MSVC AND SDRPP_MODULE_RUNTIME_OUTPUT_DIRECTORY)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${SDRPP_MODULE_RUNTIME_OUTPUT_DIRECTORY}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove -f
            "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM)
endif ()

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# Install directives
install(TARGETS ${PROJECT_NAME} DESTINATION lib/sdrpp-jp/plugins)
set_property(GLOBAL APPEND PROPERTY SDRPP_PLUGIN_TARGETS ${PROJECT_NAME})
