# Doomsday Engine - Doomsday Script Utility

cmake_minimum_required (VERSION 3.0)
project (DENG_DSCRIPT)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore QUIET)

add_executable (doomsdayscript main.cpp)
set_property (TARGET md2tool PROPERTY FOLDER Tools)
target_link_libraries (doomsdayscript Deng::libcore)
deng_target_defaults (doomsdayscript)

install (TARGETS doomsdayscript DESTINATION bin)
