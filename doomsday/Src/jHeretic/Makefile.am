lib_LTLIBRARIES = libjheretic.la

commondir = ../Common
incdir = $(top_srcdir)/Include
INCLUDES = -I$(incdir)/jHeretic -I$(incdir)/Common -I$(incdir)
DEFS = -DUNIX -DNO_FIXED_ASM -D__JHERETIC__

libjheretic_la_LDFLAGS = -module

libjheretic_la_SOURCES = AcFnLink.c H_Console.c M_misc.c P_enemy.c     \
P_map.c P_plats.c P_spec.c Sb_bar.c Am_map.c H_Main.c Mn_menu.c	       \
P_floor.c P_maputl.c P_pspr.c P_switch.c Tables.c Ct_chat.c	       \
H_Refresh.c P_ceilng.c P_inter.c P_mobj.c P_setup.c P_telept.c	       \
V_video.c H_Action.c In_lude.c P_doors.c P_lights.c P_oldsvg.c	       \
P_sight.c P_user.c p_sound.c $(commondir)/m_fixed.c		       \
$(commondir)/d_net.c $(commondir)/d_netcl.c $(commondir)/d_netsv.c     \
$(commondir)/f_infine.c $(commondir)/g_dglinit.c $(commondir)/g_game.c \
$(commondir)/g_update.c $(commondir)/hu_pspr.c $(commondir)/m_multi.c  \
$(commondir)/p_actor.c $(commondir)/p_saveg.c $(commondir)/p_start.c   \
$(commondir)/p_svtexarc.c $(commondir)/p_tick.c $(commondir)/p_view.c  \
$(commondir)/p_xgfile.c $(commondir)/p_xgline.c			       \
$(commondir)/p_xgsave.c $(commondir)/p_xgsec.c $(commondir)/r_common.c \
$(commondir)/x_hair.c
