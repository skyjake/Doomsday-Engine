# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>

greaterThan(QT_MAJOR_VERSION, 4): cache()

include(config.pri)

TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = \
    build \
    libdeng2 \
    libdeng1 \
    libshell

!deng_noclient:      SUBDIRS += libgui libappfw
!deng_noclient|macx: SUBDIRS += client

SUBDIRS += \
    server \
    plugins \
    host \
    tests

!deng_notools: SUBDIRS += tools

SUBDIRS += postbuild
