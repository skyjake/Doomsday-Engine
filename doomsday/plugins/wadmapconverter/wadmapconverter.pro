# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>

include(../config_plugin.pri)

TEMPLATE = lib
win32|macx: TARGET = dpWadMapConverter
      else: TARGET = dpwadmapconverter

VERSION = $$WADMAPCONVERTER_VERSION

INCLUDEPATH += include

HEADERS += \
    include/version.h \
    include/wadmapconverter.h

SOURCES += \
    src/load.c \
    src/wadmapconverter.c

win32 {
    QMAKE_LFLAGS += /DEF:\"$$PWD/api/dpwadmapconverter.def\"
    OTHER_FILES += api/dpwadmapconverter.def

    RC_FILE = res/wadmapconverter.rc
}

!macx {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}

macx: linkToBundledLibdeng2(dpWadMapConverter)
