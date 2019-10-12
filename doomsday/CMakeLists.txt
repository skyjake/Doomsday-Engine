# Doomsday Engine - Main project file
# This CMakeLists builds everything.

cmake_minimum_required (VERSION 3.1)
project (Doomsday)

include (cmake/Config.cmake)

add_subdirectory (sdk)
add_subdirectory (apps)
if (NOT IOS)
    if (DENG_ENABLE_TOOLS)
        add_subdirectory (tools)
    endif ()
    add_subdirectory (tests)
endif ()
add_subdirectory (post)
