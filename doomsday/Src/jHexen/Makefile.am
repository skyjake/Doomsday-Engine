lib_LTLIBRARIES = libjhexen.la

commondir = ../Common
incdir = $(top_srcdir)/Include
INCLUDES = -I$(incdir)/jHexen -I$(incdir)/Common -I$(incdir)
DEFS = -DUNIX -DNO_FIXED_ASM -D__JHEXEN__

libjhexen_la_LDFLAGS = -module

libjhexen_la_SOURCES = A_action.c AcFnLink.c Am_map.c Ct_chat.c	       \
H2_Actn.c H2_main.c HConsole.c HRefresh.c In_lude.c M_misc.c Mn_menu.c \
P_acs.c P_anim.c P_ceilng.c P_doors.c P_enemy.c P_floor.c P_inter.c    \
P_lights.c P_map.c P_maputl.c P_mobj.c P_plats.c P_pspr.c P_setup.c    \
P_sight.c P_spec.c P_switch.c P_telept.c P_things.c P_user.c Po_man.c  \
Sb_bar.c Sc_man.c Sn_sonix.c Tables.c s_sound.c sv_save.c	       \
$(commondir)/m_fixed.c $(commondir)/d_net.c $(commondir)/d_netcl.c     \
$(commondir)/d_netsv.c $(commondir)/f_infine.c			       \
$(commondir)/g_dglinit.c $(commondir)/g_game.c $(commondir)/g_update.c \
$(commondir)/hu_pspr.c $(commondir)/m_multi.c $(commondir)/p_actor.c   \
$(commondir)/p_start.c $(commondir)/p_svtexarc.c $(commondir)/p_tick.c \
$(commondir)/p_view.c $(commondir)/x_hair.c $(commondir)/r_common.c

# These common source files are currently not used in jHexen:
# $(commondir)/p_saveg.c 
# $(commondir)/p_xgfile.c $(commondir)/p_xgline.c
# $(commondir)/p_xgsave.c $(commondir)/p_xgsec.c
