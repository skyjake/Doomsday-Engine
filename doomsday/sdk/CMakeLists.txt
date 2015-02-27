# Doomsday SDK 

cmake_minimum_required (VERSION 3.1)
project (SDK)
include (../cmake/Config.cmake)

add_subdirectory (libcore)
add_subdirectory (libshell)
add_subdirectory (liblegacy)
if (DENG_ENABLE_GUI)
    add_subdirectory (libgui)
    add_subdirectory (libappfw)
endif ()
