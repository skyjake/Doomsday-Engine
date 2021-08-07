# Doomsday Engine - Tools

cmake_minimum_required (VERSION 3.1)
project (Tools)
include (../cmake/Config.cmake)

# Amethyst is expected to go to (e.g.) /usr/local/bin, while the rest
# goes to products/. Compile and install it manually beforehand.
#
# add_subdirectory (amethyst)

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
