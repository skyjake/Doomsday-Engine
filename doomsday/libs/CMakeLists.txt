# Doomsday SDK 

cmake_minimum_required (VERSION 3.1)
project (LIBS)
include (../cmake/Config.cmake)

add_subdirectory (core)
add_subdirectory (shell)
add_subdirectory (legacy)
if (DENG_ENABLE_GUI)
    add_subdirectory (gui)
    add_subdirectory (appfw)
endif ()
