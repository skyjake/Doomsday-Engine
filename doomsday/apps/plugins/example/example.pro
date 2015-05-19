# The Doomsday Engine Project
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../config_plugin.pri)

TEMPLATE = lib
TARGET   = example
VERSION  = $$EXAMPLE_VERSION

INCLUDEPATH += include

HEADERS += \
    include/version.h

SOURCES += \
    src/example.c

win32 {
    RC_FILE = res/example.rc

    deng_msvc:  QMAKE_LFLAGS += /DEF:\"$$PWD/api/example.def\"
    deng_mingw: QMAKE_LFLAGS += --def \"$$PWD/api/example.def\"

    OTHER_FILES += \
        api/example.def \
        doc/readme.txt \
        doc/LICENSE \
        $$RC_FILE
}

macx {
    fixPluginInstallId($$TARGET, 1)
    linkToBundledLiblegacy($$TARGET)
}
