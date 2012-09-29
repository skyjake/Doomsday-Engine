# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>

common_inc = $$PWD/include
common_src = $$PWD/src

INCLUDEPATH += \
    $$common_inc \
    $$DENG_LZSS_DIR/portable/include

HEADERS += \
    $$common_inc/am_map.h \
    $$common_inc/common.h \
    $$common_inc/d_net.h \
    $$common_inc/d_netcl.h \
    $$common_inc/d_netsv.h \
    $$common_inc/dmu_lib.h \
    $$common_inc/fi_lib.h \
    $$common_inc/g_common.h \
    $$common_inc/g_controls.h \
    $$common_inc/g_defs.h \
    $$common_inc/g_eventsequence.h \
    $$common_inc/g_update.h \
    $$common_inc/gl_drawpatch.h \
    $$common_inc/hu_automap.h \
    $$common_inc/hu_chat.h \
    $$common_inc/hu_inventory.h \
    $$common_inc/hu_lib.h \
    $$common_inc/hu_log.h \
    $$common_inc/hu_menu.h \
    $$common_inc/hu_msg.h \
    $$common_inc/hu_pspr.h \
    $$common_inc/hu_stuff.h \
    $$common_inc/m_argv.h \
    $$common_inc/mobj.h \
    $$common_inc/p_actor.h \
    $$common_inc/p_ceiling.h \
    $$common_inc/p_door.h \
    $$common_inc/p_floor.h \
    $$common_inc/p_inventory.h \
    $$common_inc/p_iterlist.h \
    $$common_inc/p_map.h \
    $$common_inc/p_mapsetup.h \
    $$common_inc/p_mapspec.h \
    $$common_inc/p_plat.h \
    $$common_inc/p_player.h \
    $$common_inc/p_savedef.h \
    $$common_inc/p_saveg.h \
    $$common_inc/p_saveio.h \
    $$common_inc/p_scroll.h \
    $$common_inc/p_sound.h \
    $$common_inc/p_start.h \
    $$common_inc/p_switch.h \
    $$common_inc/p_terraintype.h \
    $$common_inc/p_tick.h \
    $$common_inc/p_user.h \
    $$common_inc/p_view.h \
    $$common_inc/p_xg.h \
    $$common_inc/p_xgline.h \
    $$common_inc/p_xgsec.h \
    $$common_inc/r_common.h \
    $$common_inc/saveinfo.h \
    $$common_inc/x_hair.h \
    $$common_inc/xgclass.h

SOURCES += \
    $$common_src/am_map.c \
    $$common_src/common.c \
    $$common_src/d_net.c \
    $$common_src/d_netcl.c \
    $$common_src/d_netsv.c \
    $$common_src/dmu_lib.c \
    $$common_src/fi_lib.c \
    $$common_src/g_controls.c \
    $$common_src/g_defs.c \
    $$common_src/g_eventsequence.cpp \
    $$common_src/g_game.c \
    $$common_src/g_update.c \
    $$common_src/gl_drawpatch.c \
    $$common_src/hu_automap.c \
    $$common_src/hu_chat.c \
    $$common_src/hu_inventory.c \
    $$common_src/hu_lib.c \
    $$common_src/hu_log.c \
    $$common_src/hu_menu.c \
    $$common_src/hu_msg.c \
    $$common_src/hu_pspr.c \
    $$common_src/hu_stuff.cpp \
    $$common_src/m_ctrl.c \
    $$common_src/m_fixed.c \
    $$common_src/mobj.c \
    $$common_src/p_actor.c \
    $$common_src/p_ceiling.c \
    $$common_src/p_door.c \
    $$common_src/p_floor.c \
    $$common_src/p_inventory.c \
    $$common_src/p_iterlist.c \
    $$common_src/p_map.c \
    $$common_src/p_mapsetup.c \
    $$common_src/p_mapspec.c \
    $$common_src/p_plat.c \
    $$common_src/p_player.c \
    $$common_src/p_saveg.c \
    $$common_src/p_saveio.c \
    $$common_src/p_scroll.c \
    $$common_src/p_sound.c \
    $$common_src/p_start.c \
    $$common_src/p_switch.c \
    $$common_src/p_terraintype.c \
    $$common_src/p_tick.c \
    $$common_src/p_user.c \
    $$common_src/p_view.c \
    $$common_src/p_xgfile.c \
    $$common_src/p_xgline.c \
    $$common_src/p_xgsave.c \
    $$common_src/p_xgsec.c \
    $$common_src/r_common.c \
    $$common_src/saveinfo.c \
    $$common_src/x_hair.c
