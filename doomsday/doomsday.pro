# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>

include(config.pri)

TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS =    \
    build    \
    libdeng2 \
    libgui   \
    libdeng1 \
    libshell

!deng_noclient: SUBDIRS += client

SUBDIRS += \
    server   \
    plugins  \
    host     \
    tests

!deng_notools: SUBDIRS += tools

SUBDIRS += postbuild
