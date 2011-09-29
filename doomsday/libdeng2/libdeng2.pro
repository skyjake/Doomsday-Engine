# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

TEMPLATE = lib
TARGET = deng2
QT += core network

# Build Configuration --------------------------------------------------------

include(../config.pri)

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

# Source Files ---------------------------------------------------------------

HEADERS += \
    include/de/LegacyCore \
    include/de/legacy/legacycore.h

SOURCES += \
    src/legacy/legacycore.cpp
