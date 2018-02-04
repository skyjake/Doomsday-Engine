# Doomsday Engine -- libappfw

cmake_minimum_required (VERSION 3.1)
project (DENG_LIBAPPFW)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore)
find_package (DengGui)
find_package (DengShell)
find_package (OculusVR QUIET)

# Definitions.
add_definitions (-D__LIBAPPFW__=1)

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB SOURCES src/*.h)

deng_merge_sources (libappfw src/*.cpp)
deng_merge_sources (dialogs  src/dialogs/*.cpp)
deng_merge_sources (vr       src/vr/*.cpp)
deng_merge_sources (widgets  src/widgets/*.cpp)

deng_add_library (libappfw ${SOURCES} ${HEADERS})

target_link_libraries (libappfw
    PUBLIC Deng::libcore Deng::libgui Deng::libshell
)
deng_target_link_qt (libappfw PUBLIC Widgets OpenGL)
if (TARGET LibOVR)
    target_link_libraries (libappfw PRIVATE LibOVR)
endif ()
if (WIN32)
    # Direct2D is used for DPI information.
    target_link_libraries (libappfw PRIVATE d2d1.lib)
endif ()

deng_deploy_library (libappfw DengAppfw)

deng_cotire (libappfw src/precompiled.h)
