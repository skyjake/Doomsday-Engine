# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

# This plugin uses the full libcore C++ API.
CONFIG += dengplugin_libcore_full

include(../config_plugin.pri)

TEMPLATE = lib
TARGET   = wadmapconverter
VERSION  = $$WADMAPCONVERTER_VERSION

deng_debug: DEFINES += DENG_WADMAPCONVERTER_DEBUG

INCLUDEPATH += include

HEADERS += \
    include/id1map.h \
    include/version.h \
    include/wadmapconverter.h

SOURCES += \
    src/id1map.cpp \
    src/id1map_loadblockmap.cpp \
    src/wadmapconverter.cpp

win32 {
    RC_FILE = res/wadmapconverter.rc

    deng_msvc:  QMAKE_LFLAGS += /DEF:\"$$PWD/api/dpwadmapconverter.def\"
    deng_mingw: QMAKE_LFLAGS += --def \"$$PWD/api/dpwadmapconverter.def\"

    OTHER_FILES += api/dpwadmapconverter.def
}

!macx {
    INSTALLS += target
    target.path = $$DENG_PLUGIN_LIB_DIR
}

macx {
    fixPluginInstallId($$TARGET, 1)
    linkToBundledLibcore($$TARGET)
    linkToBundledLiblegacy($$TARGET)
}
