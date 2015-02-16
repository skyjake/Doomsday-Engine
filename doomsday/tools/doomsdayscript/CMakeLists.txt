# Doomsday Engine - Doomsday Script Utility

cmake_minimum_required (VERSION 3.0)
project (DENG_DSCRIPT)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore QUIET)

add_executable (doomsdayscript main.cpp)
target_link_libraries (doomsdayscript Deng::libcore)
deng_target_defaults (doomsdayscript)

install (TARGETS doomsdayscript DESTINATION bin)
