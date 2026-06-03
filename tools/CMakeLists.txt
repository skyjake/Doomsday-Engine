# Doomsday Engine - Tools

cmake_minimum_required (VERSION 3.10)
project (Tools)
include (../cmake/Config.cmake)

add_subdirectory (doomsdayscript)
add_subdirectory (md2tool)
add_subdirectory (savegametool)
if (DE_ENABLE_GUI AND DE_ENABLE_SHELL)
    add_subdirectory (shell)
endif ()
if (UNIX)
    add_subdirectory (dshell) # requires ncurses
endif ()
add_subdirectory (texc)
add_subdirectory (wadtool)
