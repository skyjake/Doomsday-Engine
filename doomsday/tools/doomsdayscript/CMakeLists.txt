# Doomsday Engine - Doomsday Script Utility

cmake_minimum_required (VERSION 3.1)
project (DENG_DSCRIPT)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore QUIET)

add_executable (doomsdayscript main.cpp)
set_property (TARGET doomsdayscript PROPERTY FOLDER Tools)
target_link_libraries (doomsdayscript Deng::libcore)
deng_target_defaults (doomsdayscript)

deng_install_tool (doomsdayscript)
