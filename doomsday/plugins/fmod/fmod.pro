# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

include(../config_plugin.pri)
include(../../dep_fmod.pri)

TEMPLATE = lib
TARGET = dsfmod

VERSION = $$FMOD_VERSION

INCLUDEPATH += include

HEADERS += \
    include/version.h \
    include/driver_fmod.h \
    include/fmod_cd.h \
    include/fmod_music.h \
    include/fmod_sfx.h

SOURCES += \
    src/driver_fmod.cpp \
    src/fmod_cd.cpp \
    src/fmod_music.cpp \
    src/fmod_sfx.cpp

win32 {
    RC_FILE = res/fmod.rc

    QMAKE_LFLAGS += /DEF:\"$$PWD/api/dsfmod.def\"
    OTHER_FILES += api/dsfmod.def

    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}

macx {
    doPostLink("cp -f \"$$FMOD_DIR/api/lib/libfmodex.dylib\" dsFMOD.bundle/")
    doPostLink("install_name_tool -id @executable_path/../../../dsFMOD.bundle/libfmodex.dylib dsFMOD.bundle/libfmodex.dylib")
    doPostLink("install_name_tool -change ./libfmodex.dylib @executable_path/../../../dsFMOD.bundle/libfmodex.dylib dsFMOD.bundle/dsfmod")
}
