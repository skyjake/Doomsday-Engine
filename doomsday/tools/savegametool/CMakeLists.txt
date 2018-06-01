# Doomsday Engine - Savegame Tool

cmake_minimum_required (VERSION 3.1)
project (DE_SAVEGAMETOOL)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore)
find_package (DengLegacy)
find_package (DengDoomsday)
find_package (LZSS)

file (GLOB SOURCES src/*.cpp src/*.h)

add_executable (savegametool ${SOURCES})
set_property (TARGET savegametool PROPERTY FOLDER Tools)
target_link_libraries (savegametool Deng::libcore Deng::liblegacy Deng::libdoomsday lzss)
deng_target_defaults (savegametool)

# The client expects savegametool to be available, so install in the
# "client" component.
deng_install_tool (savegametool client)
