# Doomsday Engine -- libgui

cmake_minimum_required (VERSION 3.6)
project (DE_LIBGUI)

# Apply Doomsday's configuration.
include (../../cmake/Config.cmake)

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
file (GLOB SOURCES src/stb_impl.c src/*.cpp src/*.h src/input/*.c src/text/*.m)

deng_merge_sources (audio    src/audio/*.cpp)
deng_merge_sources (dialogs  src/dialogs/*.cpp)
deng_merge_sources (graphics src/graphics/*.cpp)
deng_merge_sources (input    src/input/*.cpp)
deng_merge_sources (text     src/text/*.cpp)
deng_merge_sources (vr       src/vr/*.cpp)
deng_merge_sources (widgets  src/widgets/*.cpp)

list (APPEND SOURCES src/dialogs/filedialog_${DE_PLATFORM_SUFFIX})

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

# Link libraries.
deng_link_libraries (libgui PUBLIC DengCore)
target_link_libraries (libgui
    PUBLIC  glbinding::glbinding
    PRIVATE SDL2 stb assimp
)
if (WIN32)
    target_link_libraries (libgui PUBLIC opengl32.lib)
    # Direct2D was used for DPI information.
    # target_link_libraries (libgui PRIVATE d2d1.lib)
endif ()
if (MSYS OR CYGWIN)
    target_compile_options (libgui PUBLIC -mwindows)
    target_link_options (libgui PUBLIC -mwindows)
    target_link_libraries (libgui PUBLIC ole32)
endif ()
if (IOS)
    link_framework (libgui PRIVATE OpenGLES)
elseif (APPLE)
    link_framework (libgui PRIVATE Cocoa)
    link_framework (libgui PRIVATE OpenGL)
endif ()

deng_deploy_library (libgui DengGui)

if (WIN32)
    install (
        PROGRAMS ${ASSIMP_ROOT_DIR}/bin/libassimp.dll
                 ${glbinding_DIR}/libglbinding.dll
                 ${glbinding_DIR}/libglbinding-aux.dll
        DESTINATION bin
        COMPONENT client
    )
endif ()
if (MSYS)
    install (
        PROGRAMS ${ASSIMP_ROOT_DIR}/bin/msys-assimp-4.dll
                 ${glbinding_DIR}/msys-glbinding-3.dll
                 ${glbinding_DIR}/msys-glbinding-aux-3.dll
        DESTINATION bin
        COMPONENT client
    )
endif ()
if (CYGWIN)
    install (
        PROGRAMS ${ASSIMP_ROOT_DIR}/bin/cygassimp-4.dll
                 ${glbinding_DIR}/cygglbinding-3.dll
                 ${glbinding_DIR}/cygglbinding-aux-3.dll
        DESTINATION bin
        COMPONENT client
    )
endif ()
if (UNIX_LINUX)
    install (
        FILES ${ASSIMP_ROOT_DIR}/lib/libassimp.so.4
              ${ASSIMP_ROOT_DIR}/lib/libassimp.so.4.1.0
              ${glbinding_DIR}/lib/libglbinding.so.3
              ${glbinding_DIR}/lib/libglbinding.so.3.0.2
              ${glbinding_DIR}/lib/libglbinding-aux.so.3
              ${glbinding_DIR}/lib/libglbinding-aux.so.3.0.2
        DESTINATION lib
        COMPONENT client
    )
endif ()

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
