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

# The Windows application icon depends on glfw3 staying statically linked.
# GLFW loads a resource named GLFW_ICON (win32/resources.rc) into its window
# class at glfwInit(), but resolves it against the module *containing GLFW* --
# GetModuleHandleEx on its own address. Linked statically that module is
# sdriak.exe and the icon is found; built as a DLL the lookup hits glfw3.dll,
# finds nothing, and Windows silently substitutes the generic IDI_APPLICATION
# icon for the window, task bar and Alt-Tab. The Explorer icon keeps working
# either way, so the regression is easy to ship unnoticed -- fail at configure
# time instead of shipping it.
if (WIN32)
    sdrpp_dep_builds_shared(glfw3 _glfw3_builds_shared)
    if (_glfw3_builds_shared)
        message(FATAL_ERROR
            "glfw3 resolved to a shared build on Windows, which breaks the "
            "application icon (see the comment in deps/+glfw3/glfw3.cmake). "
            "Keep the portable:static linkage in "
            "deps/cmake/DepClassification.cmake and drop glfw3 from "
            "SDRPP_DEP_FORCE_SHARED.")
    endif ()
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
