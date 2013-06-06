# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>

include(config.pri)

TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = \
    build \
    libdeng2

!deng_noclient: SUBDIRS += libgui
    
SUBDIRS += \
    libdeng1 \
    libshell

macx|!deng_noclient: SUBDIRS += client

SUBDIRS += \
    server \
    plugins \
    host \
    tests

!deng_notools: SUBDIRS += tools

SUBDIRS += postbuild
