# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

include(../config_plugin.pri)
include(../../dep_openal.pri)

TEMPLATE = lib
TARGET   = audio_openal
VERSION  = $$OPENAL_VERSION

#DEFINES += DENG_DSOPENAL_DEBUG

INCLUDEPATH += include

HEADERS += include/version.h

SOURCES += src/driver_openal.cpp

win32 {
    RC_FILE = res/openal.rc

    QMAKE_LFLAGS += /DEF:\"$$PWD/api/dsopenal.def\"
    OTHER_FILES += api/dsopenal.def
}

!macx {
    INSTALLS += target
    target.path = $$DENG_PLUGIN_LIB_DIR
}

macx {
    linkToBundledLibdeng2(audio_openal)
    linkToBundledLibdeng1(audio_openal)
}

