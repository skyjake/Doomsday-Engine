# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>

common_inc = $$PWD/include
common_src = $$PWD/src

INCLUDEPATH += \
    $$common_inc \
    $$DENG_LZSS_DIR/portable/include

HEADERS += \
    $$common_inc/am_map.h \
    $$common_inc/animdefsparser.h \
    $$common_inc/common.h \
    $$common_inc/d_net.h \
    $$common_inc/d_netcl.h \
    $$common_inc/d_netsv.h \
    $$common_inc/dmu_archiveindex.h \
    $$common_inc/dmu_lib.h \
    $$common_inc/fi_lib.h \
    $$common_inc/g_common.h \
    $$common_inc/g_controls.h \
    $$common_inc/g_defs.h \
    $$common_inc/g_eventsequence.h \
    $$common_inc/g_update.h \
    $$common_inc/gamerules.h \
    $$common_inc/gamestatewriter.h \
    $$common_inc/gl_drawpatch.h \
    $$common_inc/hexlex.h \
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
    $$common_inc/mapstatereader.h \
    $$common_inc/mapstatewriter.h \
    $$common_inc/mobj.h \
    $$common_inc/pause.h \
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
    $$common_inc/player.h \
    $$common_inc/polyobjs.h \
    $$common_inc/r_common.h \
    $$common_inc/saveslots.h \
    $$common_inc/thingarchive.h \
    $$common_inc/thinkerinfo.h \
    $$common_inc/x_hair.h \
    $$common_inc/xgclass.h

SOURCES += \
    $$common_src/am_map.c \
    $$common_src/animdefsparser.cpp \
    $$common_src/common.c \
    $$common_src/d_net.cpp \
    $$common_src/d_netcl.cpp \
    $$common_src/d_netsv.cpp \
    $$common_src/dmu_lib.cpp \
    $$common_src/fi_lib.cpp \
    $$common_src/g_controls.c \
    $$common_src/g_defs.c \
    $$common_src/g_eventsequence.cpp \
    $$common_src/g_game.cpp \
    $$common_src/g_update.c \
    $$common_src/gamerules.cpp \
    $$common_src/gamestatewriter.cpp \
    $$common_src/gl_drawpatch.c \
    $$common_src/hexlex.cpp \
    $$common_src/hu_automap.cpp \
    $$common_src/hu_chat.c \
    $$common_src/hu_inventory.c \
    $$common_src/hu_lib.c \
    $$common_src/hu_log.c \
    $$common_src/hu_menu.cpp \
    $$common_src/hu_msg.c \
    $$common_src/hu_pspr.c \
    $$common_src/hu_stuff.cpp \
    $$common_src/m_ctrl.c \
    $$common_src/mapstatereader.cpp \
    $$common_src/mapstatewriter.cpp \
    $$common_src/mobj.cpp \
    $$common_src/pause.c \
    $$common_src/p_actor.cpp \
    $$common_src/p_ceiling.cpp \
    $$common_src/p_door.cpp \
    $$common_src/p_floor.cpp \
    $$common_src/p_inventory.cpp \
    $$common_src/p_iterlist.c \
    $$common_src/p_map.cpp \
    $$common_src/p_mapsetup.cpp \
    $$common_src/p_mapspec.c \
    $$common_src/p_plat.cpp \
    $$common_src/p_saveg.cpp \
    $$common_src/p_saveio.cpp \
    $$common_src/p_scroll.cpp \
    $$common_src/p_sound.cpp \
    $$common_src/p_start.cpp \
    $$common_src/p_switch.cpp \
    $$common_src/p_terraintype.c \
    $$common_src/p_tick.c \
    $$common_src/p_user.c \
    $$common_src/p_view.c \
    $$common_src/p_xgfile.cpp \
    $$common_src/p_xgline.cpp \
    $$common_src/p_xgsave.cpp \
    $$common_src/p_xgsec.c \
    $$common_src/player.cpp \
    $$common_src/polyobjs.cpp \
    $$common_src/r_common.c \
    $$common_src/saveslots.cpp \
    $$common_src/thingarchive.cpp \
    $$common_src/thinkerinfo.cpp \
    $$common_src/x_hair.c
