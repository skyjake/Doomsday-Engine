# Doomsday Engine -- libshell

cmake_minimum_required (VERSION 3.1)
project (DENG_LIBSHELL)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore)

# Definitions.
add_definitions (-D__LIBSHELL__)

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/shell/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.h)

deng_add_library (libshell ${SOURCES} ${HEADERS})
target_link_libraries (libshell Deng::libcore)

deng_deploy_library (libshell DengShell)

deng_cotire (libshell src/precompiled.h)
