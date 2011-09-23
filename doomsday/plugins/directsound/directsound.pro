# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../config_plugin.pri)
include(../../dep_directx.pri)
include(../../dep_eax.pri)

!win32 {
    error(DirectSound is only available on Windows.)
}

TEMPLATE = lib
TARGET = dsdirectsound

VERSION = $$DIRECTSOUND_VERSION

HEADERS += include/version.h

SOURCES += src/driver_directsound.cpp

RC_FILE = res/directsound.rc
QMAKE_LFLAGS += /DEF:\"$$PWD/api/dsdirectsound.def\"
OTHER_FILES += api/dsdirectsound.def

INSTALLS += target
target.path = $$DENG_WIN_PRODUCTS_DIR
