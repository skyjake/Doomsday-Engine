# Doomsday Engine - Tools

cmake_minimum_required (VERSION 3.0)
project (DENG_TOOLS)
include (../cmake/Config.cmake)

add_subdirectory (doomsdayscript)
add_subdirectory (md2tool)
add_subdirectory (savegametool)
if (DENG_ENABLE_GUI)
    add_subdirectory (shell)
endif ()
if (UNIX)
    add_subdirectory (shell-text) # requires ncurses
endif ()
add_subdirectory (texc)
add_subdirectory (wadtool)
