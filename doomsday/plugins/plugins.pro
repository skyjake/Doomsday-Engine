# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../config.pri)

TEMPLATE = subdirs
SUBDIRS = dehread wadmapconverter

# Games.
SUBDIRS += doom heretic hexen

# Experimental plugins.
SUBDIRS += doom64 exampleplugin

# Optional plugins.
deng_openal:!deng_noopenal {
    SUBDIRS += openal
}
deng_fmod: SUBDIRS += fmod
deng_fluidsynth: SUBDIRS += fluidsynth

# Platform-specific plugins.
win32 {
    !deng_nodirectsound: SUBDIRS += directsound
    SUBDIRS += winmm
}
