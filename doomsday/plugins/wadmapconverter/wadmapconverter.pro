# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../pluginconfig.pri)

TEMPLATE = lib
TARGET = dpwadmapconverter

VERSION = $$WADMAPCONVERTER_VERSION

INCLUDEPATH += include

HEADERS += \
    include/version.h \
    include/wadmapconverter.h

SOURCES += \
    src/load.c \
    src/wadmapconverter.c

win32 {
    RC_FILE = res/wadmapconverter.rc
}
unix:!macx {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}
