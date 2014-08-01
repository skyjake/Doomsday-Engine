# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

include(../config_plugin.pri)
include(../common/common.pri)
include(../../dep_lzss.pri)
include(../../dep_gui.pri)

TEMPLATE = lib
TARGET   = heretic
VERSION  = $$JHERETIC_VERSION

DEFINES += __JHERETIC__

gamedata.files = $$OUT_PWD/../../libheretic.pk3

macx {
    gamedata.path = Contents/Resources
    QMAKE_BUNDLE_DATA += gamedata
}
else {
    INSTALLS += gamedata
    gamedata.path = $$DENG_DATA_DIR/jheretic
}

INCLUDEPATH += include

HEADERS += \
    include/acfnlink.h \
    include/doomdata.h \
    include/doomdef.h \
    include/dstrings.h \
    include/g_game.h \
    include/h_api.h \
    include/h_config.h \
    include/h_console.h \
    include/h_event.h \
    include/h_items.h \
    include/h_main.h \
    include/h_player.h \
    include/h_refresh.h \
    include/h_stat.h \
    include/h_think.h \
    include/h_type.h \
    include/hereticv13mapstatereader.h \
    include/in_lude.h \
    include/info.h \
    include/jheretic.h \
    include/m_cheat.h \
    include/m_random.h \
    include/p_enemy.h \
    include/p_inter.h \
    include/p_lights.h \
    include/p_local.h \
    include/p_maputl.h \
    include/p_mobj.h \
    include/p_pspr.h \
    include/p_setup.h \
    include/p_spec.h \
    include/p_telept.h \
    include/r_data.h \
    include/r_defs.h \
    include/r_local.h \
    include/resource.h \
    include/st_stuff.h \
    include/tables.h \
    include/version.h

SOURCES += \
    src/acfnlink.c \
    src/h_api.c \
    src/h_console.cpp \
    src/h_main.cpp \
    src/h_refresh.cpp \
    src/hereticv13mapstatereader.cpp \
    src/in_lude.cpp \
    src/m_cheat.cpp \
    src/m_random.c \
    src/p_enemy.c \
    src/p_inter.c \
    src/p_lights.cpp \
    src/p_maputl.c \
    src/p_mobj.c \
    src/p_pspr.c \
    src/p_setup.c \
    src/p_spec.cpp \
    src/p_telept.c \
    src/st_stuff.c \
    src/tables.c

win32 {
    deng_msvc:  QMAKE_LFLAGS += /DEF:\"$$PWD/api/heretic.def\"
    deng_mingw: QMAKE_LFLAGS += --def \"$$PWD/api/heretic.def\"

    OTHER_FILES += api/heretic.def

    RC_FILE = res/heretic.rc
}

macx {
    fixPluginInstallId($$TARGET, 1)
    linkToBundledLibcore($$TARGET)
    linkToBundledLiblegacy($$TARGET)
}
