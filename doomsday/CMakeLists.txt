# Doomsday Engine - Main project file
# This CMakeLists builds everything.

cmake_minimum_required (VERSION 3.1)
project (Doomsday)

include (cmake/Config.cmake)

add_subdirectory (libs)
add_subdirectory (apps)
if (NOT IOS)
    if (DE_ENABLE_TOOLS)
        add_subdirectory (tools)
    endif ()
endif ()

add_subdirectory (post)
