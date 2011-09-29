# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

TEMPLATE = lib
TARGET = deng2

# Build Configuration --------------------------------------------------------

include(../config.pri)

# Using Qt.
QT += core network

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

INCLUDEPATH += include

# Source Files ---------------------------------------------------------------

HEADERS += \
    include/de/LegacyCore \
    include/de/legacy/legacycore.h

SOURCES += \
    src/legacy/legacycore.cpp
