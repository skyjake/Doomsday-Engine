SUBDIRS = drOpenGL dsSDLMixer dsOpenAL dpDehRead jDoom jHeretic jHexen

bin_PROGRAMS = doomsday

INCLUDES = -I$(top_srcdir)/Include
DEFS = -DUNIX -DNO_FIXED_ASM -D__DOOMSDAY__ -DNORANGECHECKING	   \
-DDENG_LIBRARY_DIR=\"$(libdir)\" -DDENG_BASE_DIR=\"$(pkgdatadir)\"
doomsday_CFLAGS = @SDL_CFLAGS@ $(LTDLINCL)
doomsday_LDFLAGS = -export-dynamic "-dlopen" self @SDL_LIBS@ -lSDL_net \
-lpng -lncurses
doomsday_LDADD = $(LIBLTDL) 
#doomsday_DEPENDENCIES = $(LIBLTDL)

doomsday_SOURCES = cl_frame.c cl_main.c cl_mobj.c cl_player.c	       \
cl_sound.c cl_world.c con_action.c con_bar.c con_bind.c con_config.c   \
con_main.c con_start.c def_main.c def_data.c def_read.c dd_dgl.c       \
dd_help.c dd_input.c dd_loop.c dd_main.c dd_plugin.c dd_wad.c dd_zip.c \
dd_zone.c gl_draw.c gl_font.c gl_main.c gl_hq2x.c gl_pcx.c gl_png.c    \
gl_tex.c gl_tga.c m_args.c m_bams.c m_filehash.c m_huffman.c m_misc.c  \
m_nodepile.c m_string.c net_demo.c net_main.c net_msg.c net_ping.c     \
p_data.c p_intercept.c p_maputil.c p_mobj.c p_particle.c p_polyob.c    \
p_sight.c p_think.c p_tick.c r_data.c r_draw.c r_extres.c r_main.c     \
r_model.c r_sky.c r_things.c r_util.c r_world.c rend_clip.c	       \
rend_decor.c rend_dyn.c rend_halo.c rend_list.c rend_main.c	       \
rend_model.c rend_particle.c rend_shadow.c rend_sky.c rend_sprite.c    \
s_cache.c s_environ.c s_logic.c s_main.c s_mus.c s_sfx.c s_wav.c       \
sv_frame.c sv_main.c sv_missile.c sv_pool.c sv_sound.c		       \
Unix/sys_console.c sys_direc.c Unix/sys_findfile.c sys_filein.c	       \
Unix/sys_input.c sys_master.c Unix/sys_mixer.c Unix/sys_musd_fmod.c    \
Unix/sys_network.c Unix/sys_path.c Unix/sys_sfxd_dummy.c	       \
Unix/sys_sfxd_loader.c sys_sock.c Unix/sys_stwin.c sys_system.c	       \
sys_timer.c tab_tables.c tab_video.c ui_main.c ui_mpi.c ui_panel.c     \
dd_pinit.c m_vector.c net_buf.c net_event.c p_bmap.c r_shadow.c	       \
rend_fakeradio.c Common/m_fixed.c Unix/dd_uinit.c Unix/lzss.c
