# Doomsday Engine - Doomsday Script Utility

cmake_minimum_required (VERSION 3.1)
project (DE_DSCRIPT)
include (../../cmake/Config.cmake)

add_executable (doomsdayscript main.cpp)
set_property (TARGET doomsdayscript PROPERTY FOLDER Tools)
deng_link_libraries (doomsdayscript PRIVATE DengCore)
deng_target_defaults (doomsdayscript)

deng_install_tool (doomsdayscript)
