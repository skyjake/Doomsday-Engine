# The Doomsday Engine Project
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>

TEMPLATE = subdirs

SUBDIRS +=  \
    shell   \
    md2tool \
    texc

win32: SUBDIRS += wadtool
