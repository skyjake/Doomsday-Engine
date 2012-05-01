# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>

include(../config_plugin.pri)

TEMPLATE = lib

win32|macx: TARGET = dpDehRead
      else: TARGET = dpdehread

VERSION = $$DEHREAD_VERSION

INCLUDEPATH += include

HEADERS += include/version.h

SOURCES += src/dehmain.c

win32 {
    QMAKE_LFLAGS += /DEF:\"$$PWD/api/dpdehread.def\"
    OTHER_FILES += api/dpdehread.def

    RC_FILE = res/dehread.rc
}

!macx {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}
