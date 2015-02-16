# Doomsday Engine -- libappfw

cmake_minimum_required (VERSION 3.0)
project (DENG_LIBAPPFW)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (Qt5Widgets)
find_package (Qt5OpenGL)
find_package (DengCore QUIET)
find_package (DengGui QUIET)
find_package (DengShell QUIET)
include (OpenGL)

# Definitions.
add_definitions (-D__LIBAPPFW__)

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.h)

deng_library (libappfw ${SOURCES} ${HEADERS})

target_link_libraries (libappfw 
    PUBLIC Deng::libcore Deng::libgui Deng::libshell Qt5::Widgets Qt5::OpenGL
)
target_link_opengl (libappfw)

deng_deploy_library (libappfw DengAppfw)
