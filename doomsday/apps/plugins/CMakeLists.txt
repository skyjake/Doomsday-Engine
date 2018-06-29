# The Doomsday Engine Project
# Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.1)
project (Plugins)
include (../../cmake/Config.cmake)

# Importers -----------------------------------------------------------------------------

add_subdirectory (importdeh)
add_subdirectory (importidtech1)
add_subdirectory (importudmf)
if (NOT IOS)
    add_subdirectory (importsave)
endif ()

# Games ---------------------------------------------------------------------------------

add_subdirectory (doom)
add_subdirectory (heretic)
add_subdirectory (hexen)
add_subdirectory (doom64)

# Audio ---------------------------------------------------------------------------------

add_subdirectory (fmod)
if (NOT TARGET audio_fmod)
    add_subdirectory (openal)
endif ()
if (NOT IOS)
    add_subdirectory (fluidsynth)
endif ()
if (WIN32)
    add_subdirectory (directsound)
    add_subdirectory (winmm)
endif ()

# Other ---------------------------------------------------------------------------------

if (DE_DEVELOPER AND NOT IOS)
    add_subdirectory (example)
endif ()
