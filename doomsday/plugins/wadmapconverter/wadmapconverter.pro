# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>

include(../config_plugin.pri)

TEMPLATE = lib
win32|macx: TARGET = dpWadMapConverter
      else: TARGET = dpwadmapconverter

VERSION = $$WADMAPCONVERTER_VERSION

deng_debug: DEFINES += DENG_WADMAPCONVERTER_DEBUG

INCLUDEPATH += include

HEADERS += \
    include/id1map_datatypes.h \
    include/id1map_load.h \
    include/id1map_util.h \
    include/maplumpinfo.h \
    include/version.h \
    include/wadmapconverter.h

SOURCES += \
    src/id1map_analyze.cpp \
    src/id1map_load.cpp \
    src/id1map_loadblockmap.cpp \
    src/id1map_util.cpp \
    src/wadmapconverter.cpp

win32 {
    RC_FILE = res/wadmapconverter.rc

    QMAKE_LFLAGS += /DEF:\"$$PWD/api/dpwadmapconverter.def\"
    OTHER_FILES += api/dpwadmapconverter.def
}

!macx {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}

macx {
    linkToBundledLibdeng2(dpWadMapConverter)
    linkToBundledLibdeng(dpWadMapConverter)
}
