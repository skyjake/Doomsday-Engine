# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../pluginconfig.pri)

TEMPLATE = lib
TARGET = dpdehread

VERSION = $$DEHREAD_VERSION

HEADERS += include/version.h

SOURCES += src/dehmain.c

win32 {
    RC_FILE = res/dehread.rc
}
unix:!macx {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}
