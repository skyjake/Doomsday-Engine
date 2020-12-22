# Doomsday Engine - Savegame Tool

cmake_minimum_required (VERSION 3.1)
project (DE_SAVEGAMETOOL)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (LZSS)

file (GLOB SOURCES src/*.cpp src/*.h)

add_executable (savegametool ${SOURCES})
set_property (TARGET savegametool PROPERTY FOLDER Tools)
deng_link_libraries (savegametool PRIVATE DengCore DengDoomsday)
target_link_libraries (savegametool PRIVATE lzss)
deng_target_defaults (savegametool)

# The client expects savegametool to be available, so install in the
# "client" component.
deng_install_tool (savegametool client)
