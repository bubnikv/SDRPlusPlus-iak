#
# GLFW — windowing/input library used by the SDRIAK GLFW backend on
# Windows/Linux/macOS. Android uses a native EGL+ANativeWindow backend and
# does not need this dependency.
#
set(_glfw3_cmake_args
    -DGLFW_BUILD_DOCS=OFF
    -DGLFW_BUILD_EXAMPLES=OFF
    -DGLFW_BUILD_TESTS=OFF
    -DGLFW_INSTALL=ON
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON)

if (MSVC)
    list(APPEND _glfw3_cmake_args -DUSE_MSVC_RUNTIME_LIBRARY_DLL=ON)
endif ()

add_cmake_project(glfw3
    URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
    URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9
    CMAKE_ARGS
        ${_glfw3_cmake_args}
)

# Upstream's Windows shared build emits glfw3dll.lib as the import lib
# alongside glfw3.dll; non-Windows uses libglfw.* — list both so the
# probe works on every platform.
sdrpp_validate_dep(glfw3
    TARGET         glfw
    LIB_NAMES      glfw3dll glfw3 glfw
    DLL_NAMES      glfw3.dll
    HEADER         glfw3.h
    INCLUDE_SUBDIR GLFW
    REQUIRES_CONFIG)
