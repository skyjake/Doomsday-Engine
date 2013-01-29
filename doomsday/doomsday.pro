# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>

TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS =    \
    build    \
    libdeng2 \
    libdeng1 \
    libshell \
    client   \
    server   \
    plugins  \
    host     \
    tests

!deng_notools: SUBDIRS += tools

SUBDIRS += postbuild
