# Doomsday Engine -- libgui

cmake_minimum_required (VERSION 3.1)
project (DENG_LIBGUI)

set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME)

#----------------------------------------------------------------------------------------
# Configure Assimp first as a static library. Doomsday's Config is not
# included yet so that Assimp's build products are not included in
# the client component.
if (DENG_ASSIMP_EMBEDDED)
    set (ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Assimp: Build tools" FORCE)
    set (ASSIMP_BUILD_TESTS OFF CACHE BOOL "Assimp: Build tests" FORCE)
    set (BUILD_SHARED_LIBS OFF CACHE BOOL "Assimp: Shared library" FORCE)
    if (APPLE OR WIN32)
        set (ASSIMP_LIB_INSTALL_DIR ${DENG_INSTALL_LIB_DIR}
            CACHE PATH "libassimp install directory" FORCE
        )
    else ()
        set (ASSIMP_LIB_INSTALL_DIR ${DENG_INSTALL_PLUGIN_DIR}
            CACHE PATH "libassimp install directory" FORCE
        )
    endif ()

    # Enable only the relevant Assimp model importers.
    set (_allAssimpFormats
        3DS AC ASE ASSBIN ASSXML B3D BVH COLLADA DXF CSM HMP IRR LWO LWS MD2 MD3 MD5
        MDC MDL NFF NDO OFF OBJ OGRE OPENGEX PLY MS3D COB BLEND IFC XGL FBX Q3D Q3BSP RAW SMD
        STL TERRAGEN 3D X)
    set (_enabledAssimpFormats 3DS COLLADA MD2 MD3 MD5 MDL OBJ BLEND FBX IRR)
    foreach (_fmt ${_allAssimpFormats})
        list (FIND _enabledAssimpFormats ${_fmt} _pos)
        if (_pos GREATER -1)
            set (_enabled YES)
        else ()
            set (_enabled NO)
        endif ()
        set (ASSIMP_BUILD_${_fmt}_IMPORTER ${_enabled} CACHE BOOL "Assimp: Enable ${_fmt} importer")
    endforeach (_fmt)

    set (CMAKE_AUTOMOC OFF)
    set (oldCFlags ${CMAKE_C_FLAGS})
    if (NOT MSVC)
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-incompatible-pointer-types")
    endif ()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-address-of-packed-member")
    endif ()
    add_subdirectory (../../external/assimp assimp)
    set_property (TARGET assimp PROPERTY FOLDER Libraries)
    set (CMAKE_C_FLAGS ${oldCFlags})
    add_definitions (-DDENG_HAVE_CUSTOMIZED_ASSIMP=1)
endif ()
#----------------------------------------------------------------------------------------

# Apply Doomsday's configuration.
include (../../cmake/Config.cmake)

# libgui uses Qt 5.4 OpenGL APIs.
qmake_query (QT_VERSION "QT_VERSION")
if (QT_VERSION VERSION_LESS 5.4)
    message (FATAL_ERROR "libgui requires Qt 5.4 or newer! You can disable all GUI features with DENG_ENABLE_GUI=OFF.")
endif ()

if (APPLE)
    # Don't change display modes on macOS/iOS. We'll instead resize the
    # frame buffer and scale up.
    set (DENG_ENABLE_DISPLAYMODE_DEFAULT OFF)
else ()
    set (DENG_ENABLE_DISPLAYMODE_DEFAULT ON)
endif ()
option (DENG_ENABLE_DISPLAYMODE "Enable/disable changing the display mode"
    ${DENG_ENABLE_DISPLAYMODE_DEFAULT}
)

# OpenGL API selection.
set (DENG_OPENGL_API "3.3" CACHE STRING "OpenGL API selection (3.3, GLES3, GLES2)")

if (IOS)
    set (DENG_OPENGL_API "GLES3" CACHE STRING "OpenGL API selection" FORCE)
endif ()

# Dependencies.
find_package (DengCore)
find_package (Assimp REQUIRED)
if (DENG_X11)
    find_package (X11 REQUIRED)
endif ()

deng_add_package (net.dengine.stdlib.gui)

# Definitions.
add_definitions (-D__LIBGUI__=1)
if (DENG_OPENGL_API STREQUAL "3.3")
    add_definitions (-DDENG_OPENGL=330)
    message (STATUS "Using OpenGL 3.3")
elseif (DENG_OPENGL_API STREQUAL "GLES3")
    add_definitions (-DDENG_OPENGL_ES=30)
    message (STATUS "Using OpenGL ES 3.0")
elseif (DENG_OPENGL_API STREQUAL "GLES2")
    add_definitions (-DDENG_OPENGL_ES=20)
    message (STATUS "Using OpenGL ES 2.0")
else ()
    message (FATAL_ERROR "Invalid value for OpenGL API: ${DENG_OPENGL_API}")
endif ()

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB SOURCES src/*.cpp src/*.h src/input/*.c)

deng_merge_sources (audio    src/audio/*.cpp)
deng_merge_sources (graphics src/graphics/*.cpp)
deng_merge_sources (input    src/input/*.cpp)
deng_merge_sources (text     src/text/*.cpp)

# Choose which DisplayMode sources to use.
list_remove_matches (SOURCES ".*/displaymode_.*")
if (NOT DENG_ENABLE_DISPLAYMODE)
    list (APPEND SOURCES src/displaymode_dummy.cpp)
else ()
    list (APPEND SOURCES src/displaymode_${DENG_PLATFORM_SUFFIX})
endif ()

# Ignore mobile-only sources.
list_remove_matches (HEADERS ".*_qml.*")
list_remove_matches (SOURCES ".*_qml.*")

deng_add_library (libgui ${SOURCES} ${HEADERS})

target_link_libraries (libgui
    PUBLIC Deng::libcore
    PRIVATE libassimp
)
deng_target_link_qt (libgui PUBLIC Gui OpenGLExtensions Widgets)

if (UNIX AND NOT APPLE)
    # The assimp target may be linked to some system libraries that
    # must be linked publicly (rather than privately like is the
    # done for libassimp).
    get_property (assimpLibs TARGET libassimp PROPERTY INTERFACE_LINK_LIBRARIES)
    target_link_libraries (libgui PUBLIC ${assimpLibs})
endif ()

if (IOS)
    link_framework (libgui PRIVATE OpenGLES)
elseif (APPLE)
    link_framework (libgui PRIVATE Cocoa)
    link_framework (libgui PRIVATE OpenGL)
endif ()

if (DENG_X11)
    target_include_directories (libgui PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries (libgui PRIVATE ${X11_LIBRARIES})
    deng_target_link_qt (libgui PRIVATE X11Extras)
    if (DENG_ENABLE_DISPLAYMODE)
        if (NOT X11_Xrandr_FOUND)
            message (FATAL_ERROR "DisplayMode requires X11 Xrandr extension (enable/disable with DENG_ENABLE_DISPLAYMODE)")
        endif ()
        if (NOT X11_xf86vmode_FOUND)
            message (FATAL_ERROR "DisplayMode requires X11 xf86vmode extension (enable/disable with DENG_ENABLE_DISPLAYMODE)")
        endif ()
        target_include_directories (libgui PRIVATE
            ${X11_Xrandr_INCLUDE_PATH}
            ${X11_xf86vmode_INCLUDE_PATH}
        )
        target_link_libraries (libgui PRIVATE
            ${X11_Xrandr_LIB}
            ${X11_Xxf86vm_LIB}
        )
    endif ()
endif ()

deng_deploy_library (libgui DengGui)

# If Assimp is part of the build, ensure that it gets compiled before libgui.
if (TARGET assimp)
    add_dependencies (libgui assimp)
    if (MSVC)
        target_link_libraries (libgui PRIVATE zlibstatic)   # from assimp project
    endif ()
endif ()

deng_cotire (libgui src/precompiled.h)
