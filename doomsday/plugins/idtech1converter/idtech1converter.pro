# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

# This plugin uses the full libcore C++ API.
CONFIG += dengplugin_libcore_full

include(../config_plugin.pri)

TEMPLATE = lib
TARGET   = idtech1converter
VERSION  = $$IDTECH1CONVERTER_VERSION

deng_debug: DEFINES += DENG_IDTECH1CONVERTER_DEBUG

INCLUDEPATH += include

HEADERS += \
    include/idtech1converter.h \
    include/hexlex.h \
    include/mapimporter.h \
    include/version.h \

SOURCES += \
    src/idtech1converter.cpp \
    src/hexlex.cpp \
    src/mapimporter.cpp \
    src/mapimporter_loadblockmap.cpp

win32 {
    RC_FILE = res/idtech1converter.rc

    deng_msvc:  QMAKE_LFLAGS += /DEF:\"$$PWD/api/dpidtech1converter.def\"
    deng_mingw: QMAKE_LFLAGS += --def \"$$PWD/api/dpidtech1converter.def\"

    OTHER_FILES += api/dpidtech1converter.def
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
