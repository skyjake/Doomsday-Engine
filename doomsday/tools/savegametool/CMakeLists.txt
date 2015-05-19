# Doomsday Engine - Savegame Tool

cmake_minimum_required (VERSION 3.1)
project (DENG_SAVEGAMETOOL)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore QUIET)
find_package (DengLegacy QUIET)
find_package (LZSS)

file (GLOB SOURCES src/*.cpp src/*.h)

add_executable (savegametool ${SOURCES})
set_property (TARGET savegametool PROPERTY FOLDER Tools)
target_link_libraries (savegametool Deng::libcore Deng::liblegacy lzss)
deng_target_defaults (savegametool)

deng_install_tool (savegametool)
