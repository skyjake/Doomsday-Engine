# The Doomsday Engine Project
# Copyright (c) 2011-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2014 Daniel Swanson <danij@dengine.net>

# This plugin uses the full libdeng2 C++ API.
CONFIG += dengplugin_libdeng2_full

include(../config_plugin.pri)

TEMPLATE = lib
TARGET   = savegameconverter
VERSION  = $$SAVEGAMECONVERTER_VERSION

INCLUDEPATH += include

HEADERS += \
    include/savegameconverter.h \
    include/version.h

SOURCES += \
    src/savegameconverter.cpp

win32 {
    QMAKE_LFLAGS += /DEF:\"$$PWD/api/dpsavegameconverter.def\"
    OTHER_FILES += api/dpsavegameconverter.def

    RC_FILE = res/savegameconverter.rc
}

!macx {
    INSTALLS += target
    target.path = $$DENG_PLUGIN_LIB_DIR
}

macx {
    fixPluginInstallId($$TARGET, 2)
    linkToBundledLibdeng2($$TARGET)
    linkToBundledLibdeng1($$TARGET)
}
