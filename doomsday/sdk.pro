# The Doomsday Engine Project
# Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>

greaterThan(QT_MAJOR_VERSION, 4): cache()

!deng_sdk: error(\"deng_sdk'" must be defined in CONFIG!)

include(config.pri)

TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = \
    build \
    libdeng2 \
    libdeng1 \
    libshell

!deng_noclient|macx {
    SUBDIRS += \
        libgui \
        libappfw 
}

SUBDIRS += \
    tests

#SUBDIRS += postbuild

