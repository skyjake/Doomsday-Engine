lib_LTLIBRARIES = libjdoom.la

commondir = ../Common
incdir = $(top_srcdir)/Include
INCLUDES = -I$(incdir)/jDoom -I$(incdir)/Common -I$(incdir) 
DEFS = -DUNIX -DNO_FIXED_ASM -D__JDOOM__

libjdoom_la_LDFLAGS = -module

libjdoom_la_SOURCES = AcFnLink.c D_Action.c am_map.c d_console.c       \
d_items.c d_main.c d_refresh.c doomdef.c doomstat.c dstrings.c	       \
hu_lib.c hu_msg.c hu_stuff.c m_cheat.c m_ctrl.c m_menu.c m_misc.c      \
m_random.c m_swap.c p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c \
p_lights.c p_map.c p_maputl.c p_mobj.c p_oldsvg.c p_plats.c p_pspr.c   \
p_setup.c p_sound.c p_spec.c p_switch.c p_telept.c p_user.c r_sky.c    \
st_lib.c st_stuff.c tables.c v_video.c wi_stuff.c		       \
$(commondir)/m_fixed.c $(commondir)/d_net.c $(commondir)/d_netcl.c     \
$(commondir)/d_netsv.c $(commondir)/f_infine.c			       \
$(commondir)/g_dglinit.c $(commondir)/g_game.c $(commondir)/g_update.c \
$(commondir)/hu_pspr.c $(commondir)/m_multi.c $(commondir)/p_actor.c   \
$(commondir)/p_saveg.c $(commondir)/p_start.c			       \
$(commondir)/p_svtexarc.c $(commondir)/p_tick.c $(commondir)/p_view.c  \
$(commondir)/p_xgfile.c $(commondir)/p_xgline.c			       \
$(commondir)/p_xgsave.c $(commondir)/p_xgsec.c $(commondir)/r_common.c \
$(commondir)/x_hair.c
