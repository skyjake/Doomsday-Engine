# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>

include(../config_plugin.pri)

!win32 {
    error(Windows Multimedia Mixing is only available on Windows.)
}

TEMPLATE = lib
TARGET = dsWinMM

VERSION = $$WINMM_VERSION

INCLUDEPATH += include

HEADERS += \
    include/cdaudio.h \
    include/dswinmm.h \
    include/midistream.h \
    include/version.h

SOURCES += \
    src/cdaudio.cpp \
    src/dswinmm.cpp \
    src/midistream.cpp

RC_FILE = res/winmm.rc

LIBS += -lwinmm

QMAKE_LFLAGS += /DEF:\"$$PWD/api/dswinmm.def\"
OTHER_FILES += api/dswinmm.def

INSTALLS += target
target.path = $$DENG_LIB_DIR
