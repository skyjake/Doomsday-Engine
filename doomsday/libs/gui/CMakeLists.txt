# Doomsday Engine -- libgui

cmake_minimum_required (VERSION 3.6)
project (DE_LIBGUI)

# Apply Doomsday's configuration.
include (../../cmake/Config.cmake)

if (APPLE)
    # Don't change display modes on macOS/iOS. We'll instead resize the
    # frame buffer and scale up.
    set (DE_ENABLE_DISPLAYMODE_DEFAULT OFF)
else ()
    set (DE_ENABLE_DISPLAYMODE_DEFAULT ON)
endif ()
option (DE_ENABLE_DISPLAYMODE "Enable/disable changing the display mode"
    ${DE_ENABLE_DISPLAYMODE_DEFAULT}
)

# OpenGL API selection.
set (DE_OPENGL_API "3.3" CACHE STRING "OpenGL API selection (3.3, GLES3, GLES2)")
if (IOS)
    set (DE_OPENGL_API "GLES3" CACHE STRING "OpenGL API selection" FORCE)
endif ()

# Dependencies.
find_package (SDL2Libs)
find_package (Stb)
find_package (glbinding REQUIRED)
find_package (Assimp)
if (DE_X11)
    find_package (X11 REQUIRED)
endif ()

deng_add_package (net.dengine.stdlib.gui)

# Definitions.
add_definitions (-D__LIBGUI__=1)

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB SOURCES src/stb_impl.c src/*.cpp src/*.h src/input/*.c)

deng_merge_sources (audio    src/audio/*.cpp)
deng_merge_sources (dialogs  src/dialogs/*.cpp)
deng_merge_sources (graphics src/graphics/*.cpp)
deng_merge_sources (input    src/input/*.cpp)
deng_merge_sources (text     src/text/*.cpp)
deng_merge_sources (vr       src/vr/*.cpp)
deng_merge_sources (widgets  src/widgets/*.cpp)

list (APPEND SOURCES src/dialogs/filedialog_${DE_PLATFORM_SUFFIX})

# Choose which DisplayMode sources to use.
list_remove_matches (SOURCES ".*/displaymode_.*")
if (NOT DE_ENABLE_DISPLAYMODE)
    list (APPEND SOURCES src/graphics/displaymode_dummy.cpp)
else ()
    list (APPEND SOURCES src/graphics/displaymode_sdl.cpp)
endif ()

deng_add_library (libgui ${SOURCES} ${HEADERS})

if (DE_OPENGL_API STREQUAL "3.3")
    target_compile_definitions (libgui PUBLIC -DDE_OPENGL=330)
    message (STATUS "Using OpenGL 3.3")
elseif (DE_OPENGL_API STREQUAL "GLES3")
    target_compile_definitions (libgui PUBLIC -DDE_OPENGL_ES=30)
    message (STATUS "Using OpenGL ES 3.0")
elseif (DE_OPENGL_API STREQUAL "GLES2")
    target_compile_definitions (libgui PUBLIC -DDE_OPENGL_ES=20)
    message (STATUS "Using OpenGL ES 2.0")
else ()
    message (FATAL_ERROR "Invalid value for OpenGL API: ${DE_OPENGL_API}")
endif ()

deng_link_libraries (libgui PUBLIC DengComms)
target_link_libraries (libgui
    PUBLIC  glbinding::glbinding
    PRIVATE SDL2 SDL2_ttf stb assimp
)
if (WIN32)
    target_link_libraries (libgui PUBLIC opengl32.lib)
    # Direct2D is used for DPI information.
    target_link_libraries (libgui PRIVATE d2d1.lib)
endif ()

# if (UNIX AND NOT APPLE)
#     # The assimp target may be linked to some system libraries that
#     # must be linked publicly (rather than privately like is the
#     # done for libassimp).
#     get_property (assimpLibs TARGET libassimp PROPERTY INTERFACE_LINK_LIBRARIES)
#     target_link_libraries (libgui PUBLIC ${assimpLibs})
# endif ()

if (IOS)
    link_framework (libgui PRIVATE OpenGLES)
elseif (APPLE)
    link_framework (libgui PRIVATE Cocoa)
    link_framework (libgui PRIVATE OpenGL)
endif ()

#if (DE_X11)
#    target_include_directories (libgui PRIVATE ${X11_INCLUDE_DIR})
#    target_link_libraries (libgui PRIVATE ${X11_LIBRARIES})
#    deng_target_link_qt (libgui PRIVATE X11Extras)
#    if (DE_ENABLE_DISPLAYMODE)
#        if (NOT X11_Xrandr_FOUND)
#            message (FATAL_ERROR "DisplayMode requires X11 Xrandr extension (enable/disable with DE_ENABLE_DISPLAYMODE)")
#        endif ()
#        if (NOT X11_xf86vmode_FOUND)
#            message (FATAL_ERROR "DisplayMode requires X11 xf86vmode extension (enable/disable with DE_ENABLE_DISPLAYMODE)")
#        endif ()
#        target_include_directories (libgui PRIVATE
#            ${X11_Xrandr_INCLUDE_PATH}
#            ${X11_xf86vmode_INCLUDE_PATH}
#        )
#        target_link_libraries (libgui PRIVATE
#            ${X11_Xrandr_LIB}
#            ${X11_Xxf86vm_LIB}
#        )
#    endif ()
#endif ()

deng_deploy_library (libgui DengGui)

# If Assimp is part of the build, ensure that it gets compiled before libgui.
# if (TARGET assimp)
#     add_dependencies (libgui assimp)
#     if (MSVC)
#         target_link_libraries (libgui PRIVATE zlibstatic)   # from assimp project
#     endif ()
# endif ()

deng_cotire (libgui src/precompiled.h)

if (DE_ENABLE_TESTS)
    set (guiTests
        test_glsandbox
        test_appfw
    )
    foreach (test ${guiTests})
        add_subdirectory (../../tests/${test} ${CMAKE_CURRENT_BINARY_DIR}/${test})
    endforeach (test)
endif ()
