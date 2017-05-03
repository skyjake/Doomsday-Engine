# The Doomsday Engine Project
# Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.1)
project (Plugins)
include (../../cmake/Config.cmake)

add_subdirectory (libgamefw)

add_subdirectory (importdeh)
add_subdirectory (importidtech1)
add_subdirectory (importudmf)
if (NOT IOS)
    add_subdirectory (importsave)
endif ()

add_subdirectory (doom)
add_subdirectory (heretic)
add_subdirectory (hexen)
add_subdirectory (doom64)

add_subdirectory (fmod)
add_subdirectory (openal)
if (NOT (WIN32 OR IOS))
    add_subdirectory (fluidsynth)
endif ()

# Platform-specific plugins.
if (WIN32)
    add_subdirectory (directsound)
    add_subdirectory (winmm)
endif ()

if (DENG_DEVELOPER)
    add_subdirectory (example)
endif ()
