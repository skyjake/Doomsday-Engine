# Doomsday Engine -- libgui

cmake_minimum_required (VERSION 3.10)
project (DE_LIBGUI)

# Apply Doomsday's configuration.
include (../../cmake/Config.cmake)

# OpenGL API selection.
set (DE_OPENGL_API "3.3" CACHE STRING "OpenGL API selection (3.3, GLES3, GLES2)")
if (IOS)
    set (DE_OPENGL_API "GLES3" CACHE STRING "OpenGL API selection" FORCE)
endif ()

# Dependencies.
find_package (SDL3Libs)
find_package (Stb)
find_package (glbinding)
find_package (Assimp)
if (NOT DE_USE_SYSTEM_ASSIMP AND TARGET assimp::assimp)
    # Promote to global so apps/ directories can reference it in deng_install_bundle_deps.
    set_target_properties (assimp::assimp PROPERTIES IMPORTED_GLOBAL TRUE)
endif ()
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

if (DE_X11 OR RASPBERRYPI)
    add_definitions (-DDE_USE_GENERIC_FILEDIALOG=1)
elseif (APPLE)
    list (APPEND SOURCES src/dialogs/filedialog_macx.mm)
    list (APPEND SOURCES src/guiapp_macx.mm)
endif ()

deng_add_library (libgui ${SOURCES} ${HEADERS})
target_precompile_headers (libgui PRIVATE "${DE_SOURCE_DIR}/cmake/pch/pch_gui.h")
# Objective-C and Objective-C++ source files cannot use the C++ PCH.
set_source_files_properties (
    src/text/fonts_macx.m
    src/dialogs/filedialog_macx.mm
    src/guiapp_macx.mm
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

if (DE_OPENGL_API STREQUAL "3.3")
    target_compile_definitions (libgui PUBLIC -DDE_OPENGL=330)
    message (STATUS "Using OpenGL 3.3")
    set (DE_HAVE_OPENGL_ES NO)
elseif (DE_OPENGL_API STREQUAL "GLES3")
    target_compile_definitions (libgui PUBLIC -DDE_OPENGL_ES=30)
    message (STATUS "Using OpenGL ES 3.0")
    set (DE_HAVE_OPENGL_ES YES)
elseif (DE_OPENGL_API STREQUAL "GLES2")
    target_compile_definitions (libgui PUBLIC -DDE_OPENGL_ES=20)
    message (STATUS "Using OpenGL ES 2.0")
    set (DE_HAVE_OPENGL_ES YES)
else ()
    message (FATAL_ERROR "Invalid value for OpenGL API: ${DE_OPENGL_API}")
endif ()

if (APPLE)
    if (CLANG_VERSION_STRING VERSION_EQUAL 12.0 OR
        CLANG_VERSION_STRING VERSION_GREATER 12.0)
        # Lots of warnings from Obj-C++ files.
        append_unique (CMAKE_CXX_FLAGS "-Wno-gnu-zero-variadic-macro-arguments")
        append_unique (CMAKE_CXX_FLAGS "-Wno-four-char-constants")
        append_unique (CMAKE_CXX_FLAGS "-Wno-deprecated-declarations")
        append_unique (CMAKE_CXX_FLAGS "-Wno-variadic-macros")
        append_unique (CMAKE_CXX_FLAGS "-Wno-zero-length-array")
        append_unique (CMAKE_CXX_FLAGS "-Wno-missing-method-return-type")
        append_unique (CMAKE_CXX_FLAGS "-Wno-availability")
        # Objective-C warnings from system headers.
        append_unique (CMAKE_C_FLAGS "-Wno-pedantic")
        append_unique (CMAKE_C_FLAGS "-Wno-extra-semi")
        append_unique (CMAKE_C_FLAGS "-Wno-gnu-zero-variadic-macro-arguments")
        append_unique (CMAKE_C_FLAGS "-Wno-four-char-constants")
        append_unique (CMAKE_C_FLAGS "-Wno-deprecated-declarations")
        append_unique (CMAKE_C_FLAGS "-Wno-variadic-macros")
        append_unique (CMAKE_C_FLAGS "-Wno-zero-length-array")
        append_unique (CMAKE_C_FLAGS "-Wno-missing-method-return-type")
        append_unique (CMAKE_C_FLAGS "-Wno-availability")
    endif ()
endif ()

# Link libraries.
deng_link_libraries (libgui PUBLIC DengCore)
target_link_libraries (libgui PRIVATE SDL3 stb assimp)
if (NOT DE_HAVE_OPENGL_ES)
    target_compile_definitions (libgui PRIVATE DE_GLBINDING_VERSION=${DE_GLBINDING_VERSION})
    if (DE_USE_SYSTEM_GLBINDING)
        target_link_libraries (libgui PUBLIC glbinding)
    else ()
        target_link_libraries (libgui PUBLIC glbinding::glbinding)
    endif ()
endif ()
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
if (RASPBERRYPI)
    target_link_libraries (libgui PRIVATE bcm)
endif ()

deng_deploy_library (libgui DengGui)

if (WIN32 OR MSYS OR CYGWIN)
    install (
        PROGRAMS $<TARGET_FILE:assimp::assimp>
                 ${glbinding_DIR}/libglbinding.dll
                 ${glbinding_DIR}/libglbinding-aux.dll
        DESTINATION bin
        COMPONENT client
    )
endif ()
if (UNIX_LINUX AND NOT DE_USE_SYSTEM_ASSIMP)
    install (
        FILES $<TARGET_FILE:assimp::assimp>
        DESTINATION ${DE_INSTALL_LIB_DIR}
        COMPONENT client
    )
endif ()
if (UNIX_LINUX AND NOT DE_USE_SYSTEM_GLBINDING)
    install (
        FILES ${glbinding_DIR}/lib/libglbinding.so.2
              ${glbinding_DIR}/lib/libglbinding.so.2.1.4
        DESTINATION ${DE_INSTALL_LIB_DIR}
        COMPONENT client
    )
endif ()

if (DE_ENABLE_TESTS)
    set (guiTests
        test_glsandbox
        test_appfw
    )
    foreach (test ${guiTests})
        add_subdirectory (../../tests/${test} ${CMAKE_CURRENT_BINARY_DIR}/${test})
    endforeach (test)
endif ()
