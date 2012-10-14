# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>

# This plugin uses the full libdeng2 C++ API.
CONFIG += dengplugin_libdeng2_full

include(../config_plugin.pri)

TEMPLATE = lib
TARGET   = dehread
VERSION  = $$DEHREAD_VERSION

# TODO: the dependencies to internal headers should be removed
# (see comment in dehread.cpp)
INCLUDEPATH += include \
    ../../engine/portable/include \
    ../../engine/portable/include/render

HEADERS += include/dehread.h \
    include/dehreader.h \
    include/dehreader_util.h \
    include/info.h \
    include/version.h

SOURCES += src/dehread.cpp \
    src/dehreader.cpp \
    src/dehreader_util.cpp \
    src/info.cpp

win32 {
    QMAKE_LFLAGS += /DEF:\"$$PWD/api/dpdehread.def\"
    OTHER_FILES += api/dpdehread.def

    RC_FILE = res/dehread.rc
}

!macx {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}

macx {
    linkToBundledLibdeng2(dehread)
    linkToBundledLibdeng(dehread)
}
