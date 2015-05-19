# The Doomsday Engine Project
# Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.1)
project (Plugins)
include (../../cmake/Config.cmake)

if (DENG_DEVELOPER)
    add_subdirectory (example)
endif ()

add_subdirectory (idtech1converter)
add_subdirectory (savegameconverter)
add_subdirectory (dehread)

add_subdirectory (doom)
add_subdirectory (heretic)
add_subdirectory (hexen)
add_subdirectory (doom64)

add_subdirectory (fmod)
add_subdirectory (openal)
if (NOT WIN32)
    add_subdirectory (fluidsynth)
endif ()

# Platform-specific plugins.
if (WIN32)
    add_subdirectory (directsound)
    add_subdirectory (winmm)
endif ()
