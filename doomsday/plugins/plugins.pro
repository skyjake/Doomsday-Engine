# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../config.pri)

TEMPLATE = subdirs
SUBDIRS = dehread wadmapconverter

# Games.
SUBDIRS += jdoom jheretic jhexen

# Experimental games.
SUBDIRS += jdoom64

# Optional plugins.
deng_openal:!deng_noopenal {
    SUBDIRS += openal
}
deng_fmod: SUBDIRS += fmod
deng_fluidsynth: SUBDIRS += fluidsynth

# Platform-specific plugins.
win32: SUBDIRS += directsound winmm
