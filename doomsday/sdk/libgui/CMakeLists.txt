# Doomsday Engine -- libgui

cmake_minimum_required (VERSION 3.1)
project (DENG_LIBGUI)
include (../../cmake/Config.cmake)

set (ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Build assimp tools")
set (ASSIMP_LIB_INSTALL_DIR ${DENG_INSTALL_PLUGIN_DIR}
    CACHE PATH "libassimp install directory" FORCE
)
add_subdirectory (../../external/assimp assimp)

if (APPLE)
    # Don't change display modes on OS X. We'll instead resize the 
    # framebuffer  and scale up.
    set (DENG_ENABLE_DISPLAYMODE_DEFAULT OFF)
else ()
    set (DENG_ENABLE_DISPLAYMODE_DEFAULT ON)
endif ()
option (DENG_ENABLE_DISPLAYMODE "Enable/disable changing the display mode"
    ${DENG_ENABLE_DISPLAYMODE_DEFAULT}
)

# Dependencies.
find_package (DengCore)
find_package (Assimp REQUIRED)
include (OpenGL)

if (DENG_X11)
    find_package (X11 REQUIRED)
endif ()

deng_add_package (net.dengine.stdlib.gui)

# Definitions.
add_definitions (-D__LIBGUI__)

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.h)

# Choose which DisplayMode sources to use.
list_remove_matches (SOURCES ".*/displaymode_.*")
if (NOT DENG_ENABLE_DISPLAYMODE)
    list (APPEND SOURCES src/displaymode_dummy.cpp)
else ()
    list (APPEND SOURCES src/displaymode_${DENG_PLATFORM_SUFFIX})
endif ()

deng_add_library (libgui ${SOURCES} ${HEADERS})

target_link_libraries (libgui 
    PUBLIC Deng::libcore
    PRIVATE opengl libassimp 
)
deng_target_link_qt (libgui PUBLIC Gui OpenGL)

if (APPLE)
    link_framework (libgui PRIVATE Cocoa)
endif ()

if (DENG_X11)
    target_include_directories (libgui PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries (libgui PRIVATE ${X11_LIBRARIES})
    deng_target_link_qt (libgui PRIVATE X11Extras)
    if (DENG_ENABLE_DISPLAYMODE)
        if (NOT X11_Xrandr_FOUND)
            message (FATAL_ERROR "DisplayMode requires X11 Xrandr extension")
        endif ()
        if (NOT X11_xf86vmode_FOUND)
            message (FATAL_ERROR "DisplayMode requires X11 xf86vmode extension")
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
endif ()

deng_cotire (libgui src/precompiled.h)
