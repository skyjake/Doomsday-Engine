# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>

TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = \
    build \
    external \
    libdeng2 libdeng \
    engine plugins host \
    postbuild
