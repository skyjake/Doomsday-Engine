# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

TEMPLATE = lib
TARGET = deng2

# Build Configuration --------------------------------------------------------

include(../config.pri)

DEFINES += __DENG2__

# Using Qt.
QT += core network

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

INCLUDEPATH += include

# Source Files ---------------------------------------------------------------

HEADERS += \
    include/de/c_wrapper.h \
    include/de/libdeng2.h \
    include/de/LegacyCore \
    include/de/legacy/legacycore.h

SOURCES += \
    src/c_wrapper.cpp \
    src/legacy/legacycore.cpp
