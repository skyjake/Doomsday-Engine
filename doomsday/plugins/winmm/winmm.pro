# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

include(../config_plugin.pri)

!win32 {
    error(Windows Multimedia Mixing is only available on Windows.)
}

TEMPLATE = lib
TARGET   = audio_winmm
VERSION  = $$WINMM_VERSION

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

deng_msvc:  QMAKE_LFLAGS += /DEF:\"$$PWD/api/dswinmm.def\"
deng_mingw: QMAKE_LFLAGS += --def \"$$PWD/api/dswinmm.def\"
OTHER_FILES += api/dswinmm.def
