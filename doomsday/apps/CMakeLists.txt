# Doomsday Engine - Applications

cmake_minimum_required (VERSION 3.1)
project (Apps)
include (../cmake/Config.cmake)

if (DE_ENABLE_SERVER AND NOT IOS)
    add_subdirectory (server)
endif ()
if (DE_ENABLE_GLOOMED)
    add_subdirectory (gloom)
    #add_subdirectory (gloomed)
endif ()
if (DE_ENABLE_GUI)
    add_subdirectory (client)
endif ()
