# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>

TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS =    \
    build    \
    libdeng2 \
    libdeng1 \
    engine   \
    server   \
    plugins  \
    host     \
    tests

!deng_notools {
    SUBDIRS += \
        ../tools/md2tool \
        ../tools/texc
    win32: SUBDIRS += ../tools/wadtool
}

SUBDIRS += postbuild
