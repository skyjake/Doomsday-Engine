lib_LTLIBRARIES = libjhexen.la

commondir = ../Common
incdir = $(top_srcdir)/Include
INCLUDES = -I$(incdir)/jHexen -I$(incdir)/Common -I$(incdir)
DEFS = -DUNIX -D@FIXED_IMPL@ -D__JHEXEN__

libjhexen_la_LDFLAGS = -module

libjhexen_la_SOURCES = A_action.c AcFnLink.c H2_Actn.c H2_main.c       \
HConsole.c HRefresh.c In_lude.c M_misc.c P_acs.c P_anim.c P_ceilng.c   \
P_doors.c P_enemy.c P_floor.c P_inter.c P_lights.c P_map.c P_maputl.c  \
P_mobj.c P_plats.c P_pspr.c P_setup.c P_sight.c P_spec.c P_switch.c    \
P_telept.c P_things.c P_user.c Po_man.c Sc_man.c Sn_sonix.c Tables.c   \
s_sound.c sv_save.c $(commondir)/m_fixed.c $(commondir)/d_net.c	       \
$(commondir)/d_netcl.c $(commondir)/d_netsv.c $(commondir)/f_infine.c  \
$(commondir)/g_dglinit.c $(commondir)/g_game.c $(commondir)/g_update.c \
$(commondir)/hu_pspr.c $(commondir)/m_multi.c $(commondir)/p_actor.c   \
$(commondir)/p_start.c $(commondir)/p_svtexarc.c $(commondir)/p_tick.c \
$(commondir)/p_view.c $(commondir)/x_hair.c $(commondir)/r_common.c    \
$(commondir)/am_map.c $(commondir)/hu_lib.c $(commondir)/hu_msg.c      \
$(commondir)/hu_stuff.c $(commondir)/m_ctrl.c $(commondir)/mn_menu.c   \
$(commondir)/st_lib.c


# These common source files are currently not used in jHexen:
# $(commondir)/p_saveg.c 
# $(commondir)/p_xgfile.c $(commondir)/p_xgline.c
# $(commondir)/p_xgsave.c $(commondir)/p_xgsec.c
