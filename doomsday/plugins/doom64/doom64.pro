# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

include(../config_plugin.pri)
include(../common/common.pri)
include(../../dep_lzss.pri)

TEMPLATE = lib
TARGET   = doom64
VERSION  = $$JDOOM64_VERSION

DEFINES += __JDOOM64__

gamedata.files = $$OUT_PWD/../../libdoom64.pk3

macx {
    gamedata.path = Contents/Resources

    QMAKE_BUNDLE_DATA += gamedata
}
else {
    INSTALLS += target gamedata

    target.path = $$DENG_PLUGIN_LIB_DIR
    gamedata.path = $$DENG_DATA_DIR/jdoom64
}

INCLUDEPATH += include

HEADERS += \
    include/acfnlink.h \
    include/d_api.h \
    include/d_config.h \
    include/d_console.h \
    include/d_englsh.h \
    include/d_items.h \
    include/d_main.h \
    include/d_player.h \
    include/d_refresh.h \
    include/d_think.h \
    include/doomdef.h \
    include/doomtype.h \
    include/dstrings.h \
    include/g_ctrl.h \
    include/g_game.h \
    include/info.h \
    include/jdoom64.h \
    include/m_cheat.h \
    include/m_random.h \
    include/p_enemy.h \
    include/p_inter.h \
    include/p_lights.h \
    include/p_maputl.h \
    include/p_mobj.h \
    include/p_pspr.h \
    include/p_setup.h \
    include/p_spec.h \
    include/p_telept.h \
    include/r_defs.h \
    include/st_stuff.h \
    include/tables.h \
    include/version.h \
    include/wi_stuff.h

SOURCES += \
    src/acfnlink.c \
    src/d_api.c \
    src/d_console.c \
    src/d_items.c \
    src/d_main.c \
    src/d_refresh.c \
    src/g_ctrl.c \
    src/m_cheat.c \
    src/m_random.c \
    src/p_enemy.c \
    src/p_inter.c \
    src/p_lights.c \
    src/p_maputl.c \
    src/p_mobj.c \
    src/p_pspr.c \
    src/p_setup.c \
    src/p_spec.c \
    src/p_telept.c \
    src/st_stuff.c \
    src/tables.c \
    src/wi_stuff.c \

win32 {
    QMAKE_LFLAGS += /DEF:\"$$PWD/api/doom64.def\"
    OTHER_FILES += api/doom64.def

    RC_FILE = res/doom64.rc
}

macx {
    fixPluginInstallId($$TARGET, 1)
    linkToBundledLibdeng2($$TARGET)
    linkToBundledLibdeng($$TARGET)
}

