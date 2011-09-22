# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../config_plugin.pri)

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

    INSTALLS += target
    target.path = $$DENG_WIN_PRODUCTS_DIR
}
unix:!macx {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}
