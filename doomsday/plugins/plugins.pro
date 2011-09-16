# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../config.pri)

TEMPLATE = subdirs
SUBDIRS = dehread wadmapconverter

# Games.
SUBDIRS += jdoom jheretic jhexen

# Experimental games.
SUBDIRS += jdoom64

# Optional plugins.
deng_openal {
    SUBDIRS += openal
}

# Platform-specific plugins.
win32 {
    SUBDIRS += directsound winmm
}
