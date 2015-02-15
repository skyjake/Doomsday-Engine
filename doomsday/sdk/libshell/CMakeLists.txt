# Doomsday Engine -- libshell

cmake_minimum_required (VERSION 3.0)
project (DENG_LIBSHELL)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (Qt5Core)
find_package (DengCore QUIET)

# Definitions.
add_definitions (-D__LIBSHELL__)

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/shell/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.h)

deng_library (deng_shell ${SOURCES} ${HEADERS})
target_link_libraries (deng_shell Deng::deng_core)

deng_deploy_library (deng_shell DengShell)
