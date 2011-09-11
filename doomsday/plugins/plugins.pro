# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../config.pri)

TEMPLATE = subdirs
SUBDIRS = dehread wadmapconverter

contains(DENG_PLUGINS, openal) {
    SUBDIRS += openal
}

# Games.
SUBDIRS += jdoom jheretic jhexen

# Experimental games.
SUBDIRS += jdoom64

# Platform-specific plugins.
win32 {
    SUBDIRS += directsound winmm
}

SUBDIRS -= jheretic jhexen jdoom64
