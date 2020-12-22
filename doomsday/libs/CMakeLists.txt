# Doomsday SDK 

cmake_minimum_required (VERSION 3.1)
project (LIBS)
include (../cmake/Config.cmake)

add_subdirectory (core)
add_subdirectory (doomsday)
add_subdirectory (gamekit)
if (DE_ENABLE_GUI)
    add_subdirectory (gui)
    add_subdirectory (gloom)
    add_subdirectory (doomsdaygui) # static lib: common libdoomsday-dependent GUI stuff
endif ()
