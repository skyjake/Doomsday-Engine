# Doomsday Engine -- libshell

cmake_minimum_required (VERSION 3.1)
project (DE_LIBSHELL)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore)

# Definitions.
add_definitions (-D__LIBSHELL__=1)

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/shell/*)
file (GLOB SOURCES src/*.h)

deng_merge_sources (libshell src/*.cpp)

deng_add_library (libshell ${SOURCES} ${HEADERS})
target_link_libraries (libshell PUBLIC Deng::libcore)

deng_deploy_library (libshell DengShell)

deng_cotire (libshell src/precompiled.h)
