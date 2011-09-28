# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../config_plugin.pri)
include(../../dep_openal.pri)

TEMPLATE = lib
TARGET = dsopenal

VERSION = $$OPENAL_VERSION

INCLUDEPATH += include

HEADERS += include/version.h

SOURCES += src/driver_openal.c

win32 {
    RC_FILE = res/openal.rc

    QMAKE_LFLAGS += /DEF:\"$$PWD/api/dsopenal.def\"
    OTHER_FILES += api/dsopenal.def
}
