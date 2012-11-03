# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>

include(../config_plugin.pri)
include(../common/common.pri)
include(../../dep_lzss.pri)

TEMPLATE = lib
TARGET   = hexen
VERSION  = $$JHEXEN_VERSION

DEFINES += __JHEXEN__

gamedata.files = $$OUT_PWD/../../libhexen.pk3

macx {
    gamedata.path = Contents/Resources

    QMAKE_BUNDLE_DATA += gamedata
}
else {
    INSTALLS += target gamedata

    target.path = $$DENG_PLUGIN_LIB_DIR
    gamedata.path = $$DENG_DATA_DIR/jhexen
}

INCLUDEPATH += include

HEADERS += \
    include/a_action.h \
    include/acfnlink.h \
    include/dstrings.h \
    include/g_ctrl.h \
    include/g_game.h \
    include/h2def.h \
    include/in_lude.h \
    include/info.h \
    include/jhexen.h \
    include/m_cheat.h \
    include/m_random.h \
    include/p_acs.h \
    include/p_anim.h \
    include/p_enemy.h \
    include/p_inter.h \
    include/p_lights.h \
    include/p_local.h \
    include/p_mapinfo.h \
    include/p_maputl.h \
    include/p_mobj.h \
    include/p_pillar.h \
    include/p_pspr.h \
    include/p_setup.h \
    include/p_spec.h \
    include/p_telept.h \
    include/p_things.h \
    include/p_waggle.h \
    include/po_man.h \
    include/r_defs.h \
    include/r_local.h \
    include/s_sequence.h \
    include/sc_man.h \
    include/st_stuff.h \
    include/textdefs.h \
    include/version.h \
    include/x_api.h \
    include/x_config.h \
    include/x_console.h \
    include/x_event.h \
    include/x_items.h \
    include/x_main.h \
    include/x_player.h \
    include/x_refresh.h \
    include/x_state.h \
    include/x_think.h \
    include/xddefs.h

SOURCES += \
    src/a_action.c \
    src/acfnlink.c \
    src/g_ctrl.c \
    src/h2_main.c \
    src/hconsole.c \
    src/hrefresh.c \
    src/in_lude.c \
    src/m_cheat.c \
    src/m_random.c \
    src/p_acs.c \
    src/p_anim.c \
    src/p_enemy.c \
    src/p_inter.c \
    src/p_lights.c \
    src/p_mapinfo.c \
    src/p_maputl.c \
    src/p_mobj.c \
    src/p_pillar.c \
    src/p_pspr.c \
    src/p_setup.c \
    src/p_spec.c \
    src/p_telept.c \
    src/p_things.c \
    src/p_waggle.c \
    src/po_man.c \
    src/sc_man.c \
    src/sn_sonix.c \
    src/st_stuff.c \
    src/tables.c \
    src/x_api.c

win32 {
    QMAKE_LFLAGS += /DEF:\"$$PWD/api/jhexen.def\"
    OTHER_FILES += api/jhexen.def

    RC_FILE = res/jhexen.rc
}

macx {
    fixPluginInstallId($$TARGET, 1)
    linkToBundledLibdeng2($$TARGET)
    linkToBundledLibdeng($$TARGET)
}
