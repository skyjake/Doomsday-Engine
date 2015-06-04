# Doomsday Engine -- libappfw

cmake_minimum_required (VERSION 3.1)
project (DENG_LIBAPPFW)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore)
find_package (DengGui)
find_package (DengShell)
find_package (OculusVR QUIET)
include (OpenGL)

# Definitions.
add_definitions (-D__LIBAPPFW__)

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.h)

deng_add_library (libappfw ${SOURCES} ${HEADERS})

target_link_libraries (libappfw 
    PUBLIC Deng::libcore Deng::libgui Deng::libshell
    PRIVATE opengl
)
deng_target_link_qt (libappfw PUBLIC Widgets OpenGL)
if (TARGET LibOVR)
    target_link_libraries (libappfw PRIVATE LibOVR)
endif ()

deng_deploy_library (libappfw DengAppfw)

deng_cotire (libappfw src/precompiled.h)
