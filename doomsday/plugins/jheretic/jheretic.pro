# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>

include(../config_plugin.pri)
include(../common/common.pri)
include(../../dep_lzss.pri)

TEMPLATE = lib
TARGET   = heretic
VERSION  = $$JHERETIC_VERSION

DEFINES += __JHERETIC__

gamedata.files = $$OUT_PWD/../../jheretic.pk3

macx {
    gamedata.path = Contents/Resources

    QMAKE_BUNDLE_DATA += gamedata
}
else {
    INSTALLS += target gamedata

    target.path = $$DENG_PLUGIN_LIB_DIR
    gamedata.path = $$DENG_DATA_DIR/jheretic
}

INCLUDEPATH += include

HEADERS += \
    include/acfnlink.h \
    include/doomdata.h \
    include/doomdef.h \
    include/dstrings.h \
    include/g_ctrl.h \
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
    include/p_oldsvg.h \
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
    src/g_ctrl.c \
    src/h_api.c \
    src/h_console.c \
    src/h_main.c \
    src/h_refresh.c \
    src/in_lude.c \
    src/m_cheat.c \
    src/m_random.c \
    src/p_enemy.c \
    src/p_inter.c \
    src/p_lights.c \
    src/p_maputl.c \
    src/p_mobj.c \
    src/p_oldsvg.c \
    src/p_pspr.c \
    src/p_setup.c \
    src/p_spec.c \
    src/p_telept.c \
    src/st_stuff.c \
    src/tables.c

win32 {
    QMAKE_LFLAGS += /DEF:\"$$PWD/api/jheretic.def\"
    OTHER_FILES += api/jheretic.def

    RC_FILE = res/jheretic.rc
}

macx {
    linkToBundledLibdeng2(heretic)
    linkToBundledLibdeng(heretic)
}
