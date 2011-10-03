# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

TEMPLATE = lib
TARGET = deng2

# Build Configuration --------------------------------------------------------

include(../config.pri)

# Using Qt.
QT += core network

DEFINES += __DENG2__

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

*-g++ {
    # Enable strict warnings.
    QMAKE_CXXFLAGS_WARN_ON *= -Wall -Wextra -pedantic
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
