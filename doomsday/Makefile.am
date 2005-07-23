SUBDIRS = libltdl Src

DISTCLEANFILES = *~

#
# TODO: Having all the headers here is perhaps not the best solution.
#

Headers = Include/cl_def.h Include/dd_api.h Include/LZSS.h	       \
Include/Common/d_netcl.h Include/Common/d_net.h			       \
Include/Common/d_netsv.h Include/Common/f_infine.h		       \
Include/Common/g_common.h Include/Common/g_dgl.h		       \
Include/Common/g_update.h Include/Common/hu_pspr.h		       \
Include/Common/p_actor.h Include/Common/p_saveg.h		       \
Include/Common/p_ticcmd.h Include/Common/p_start.h		       \
Include/Common/p_svtexarc.h Include/Common/p_view.h		       \
Include/Common/p_xg.h Include/Common/p_xgline.h			       \
Include/Common/p_xgsec.h Include/Common/r_common.h		       \
Include/Common/x_hair.h Include/Common/xgclass.h Include/cl_frame.h    \
Include/Common/am_map.h Include/Common/hu_lib.h			       \
Include/Common/hu_msg.h Include/Common/hu_stuff.h		       \
Include/Common/m_swap.h Include/Common/st_lib.h Include/cl_mobj.h      \
Include/cl_player.h Include/cl_sound.h Include/cl_world.h	       \
Include/con_action.h Include/con_bar.h Include/con_bind.h	       \
Include/con_config.h Include/con_decl.h Include/con_main.h	       \
Include/con_start.h Include/m_misc.h Include/dd_def.h Include/dd_dgl.h \
Include/dd_help.h Include/dd_input.h Include/dd_loop.h		       \
Include/dd_main.h Include/dd_pinit.h Include/dd_plugin.h	       \
Include/dd_share.h Include/dd_types.h Include/dd_version.h	       \
Include/dd_wad.h Include/dd_winit.h Include/dd_zip.h Include/dd_zone.h \
Include/de_audio.h Include/de_base.h Include/de_console.h	       \
Include/de_defs.h Include/de_graphics.h Include/de_misc.h	       \
Include/de_network.h Include/de_platform.h Include/de_play.h	       \
Include/de_refresh.h Include/de_render.h Include/de_system.h	       \
Include/de_ui.h Include/def_data.h Include/def_main.h		       \
Include/def_share.h Include/dglib.h Include/doomsday.h		       \
Include/gl_pcx.h Include/gl_draw.h Include/gl_font.h Include/gl_hq2x.h \
Include/gl_main.h Include/gl_model.h Include/gl_png.h Include/gl_tex.h \
Include/gl_tga.h Include/m_args.h Include/m_bams.h		       \
Include/m_filehash.h Include/m_huffman.h Include/sv_def.h	       \
Include/m_nodepile.h Include/m_profiler.h Include/m_string.h	       \
Include/m_vector.h Include/net_buf.h Include/net_demo.h		       \
Include/net_event.h Include/net_main.h Include/net_msg.h	       \
Include/p_bmap.h Include/r_main.h Include/p_data.h		       \
Include/p_intercept.h Include/p_maputil.h Include/p_mobj.h	       \
Include/p_particle.h Include/p_polyob.h Include/p_sight.h	       \
Include/p_think.h Include/p_tick.h Include/png.h Include/pngconf.h     \
Include/r_data.h Include/r_draw.h Include/r_extres.h Include/r_lgrid.h \
Include/r_model.h Include/r_shadow.h Include/r_sky.h		       \
Include/r_things.h Include/r_util.h Include/r_world.h		       \
Include/rend_bias.h Include/rend_clip.h Include/rend_decor.h	       \
Include/rend_dyn.h Include/rend_fakeradio.h Include/rend_halo.h	       \
Include/rend_list.h Include/rend_main.h Include/rend_model.h	       \
Include/rend_particle.h Include/rend_shadow.h Include/rend_sky.h       \
Include/rend_sprite.h Include/s_cache.h Include/s_environ.h	       \
Include/s_logic.h Include/s_main.h Include/s_mus.h Include/s_sfx.h     \
Include/s_wav.h Include/sv_frame.h Include/sv_missile.h		       \
Include/sv_pool.h Include/sv_sound.h Include/sys_audio.h	       \
Include/sys_console.h Include/sys_direc.h Include/sys_file.h	       \
Include/sys_findfile.h Include/sys_input.h Include/sys_master.h	       \
Include/sys_mixer.h Include/sys_musd.h Include/sys_musd_fmod.h	       \
Include/sys_musd_win.h Include/sys_network.h Include/sys_sfxd.h	       \
Include/sys_sfxd_ds.h Include/sys_sfxd_dummy.h			       \
Include/sys_sfxd_loader.h Include/sys_sock.h Include/sys_stwin.h       \
Include/sys_system.h Include/sys_timer.h Include/tab_anorms.h	       \
Include/template.h Include/ui_main.h Include/ui_mpi.h		       \
Include/ui_panel.h Include/zconf.h Include/zlib.h		       \
Include/Unix/sys_findfile.h Include/Unix/sys_path.h		       \
Include/drD3D/window.h Include/drD3D/box.h Include/drD3D/drD3D.h       \
Include/drOpenGL/atiext.h Include/drOpenGL/drOpenGL.h		       \
Src/jDoom/AcFnLink.h Include/jDoom/D_Action.h Include/jDoom/Mn_def.h   \
Include/jDoom/SndIdx.h Include/jDoom/d_config.h			       \
Include/jDoom/d_console.h Include/jDoom/d_englsh.h		       \
Include/jDoom/d_event.h Include/jDoom/d_french.h		       \
Include/jDoom/d_items.h Include/jDoom/d_main.h Include/jDoom/d_netJD.h \
Include/jDoom/d_player.h Include/jDoom/d_think.h		       \
Include/jDoom/d_ticcmd.h Include/jDoom/doomdata.h		       \
Include/jDoom/doomdef.h Include/jDoom/doomstat.h		       \
Include/jDoom/doomtype.h Include/jDoom/dstrings.h		       \
Include/jDoom/g_game.h  \
Include/jDoom/info.h Include/jDoom/m_argv.h Include/jDoom/m_cheat.h    \
Include/jDoom/m_menu.h Include/jDoom/m_random.h \
Include/jDoom/oldinfo.h Include/jDoom/p_inter.h \
Include/jDoom/p_local.h Include/jDoom/p_mobj.h			       \
Include/jDoom/p_oldsvg.h Include/jDoom/p_pspr.h			       \
Include/jDoom/p_setup.h Include/jDoom/p_spec.h Include/jDoom/r_data.h  \
Include/jDoom/r_defs.h Include/jDoom/r_local.h Include/jDoom/r_sky.h   \
Include/jDoom/r_state.h Include/jDoom/s_sound.h  \
Include/jDoom/st_stuff.h Include/jDoom/tables.h			       \
Include/jDoom/version.h			       \
Include/jDoom/wi_stuff.h Include/jDoom/x_hair.h Include/jDoom/m_ctrl.h \
Include/jHeretic/AcFnLink.h		       \
		       \
Include/jHeretic/Doomdata.h Include/jHeretic/Doomdef.h		       \
Include/jHeretic/Dstrings.h Include/jHeretic/G_game.h		       \
Include/jHeretic/H_Action.h Include/jHeretic/Info.h		       \
Include/jHeretic/Mn_def.h Include/jHeretic/P_local.h		       \
Include/jHeretic/P_spec.h Include/jHeretic/R_local.h		       \
Include/jHeretic/S_sound.h Include/jHeretic/Sounds.h		       \
Include/jHeretic/Soundst.h Include/jHeretic/p_oldsvg.h		       \
Include/jHeretic/resource.h 		       \
Include/jHeretic/version.h Include/jHexen/AcFnLink.h		       \
Include/jHeretic/h_config.h Include/jHeretic/m_ctrl.h		       \
Include/jHeretic/st_stuff.h 		       \
		       \
Include/jHexen/dstrings.h Include/jHexen/h2_actn.h		       \
Include/jHexen/h2def.h Include/jHexen/info.h Include/jHexen/mn_def.h   \
Include/jHexen/p_local.h Include/jHexen/p_spec.h		       \
Include/jHexen/r_local.h 		       \
Include/jHexen/sounds.h Include/jHexen/soundst.h		       \
Include/jHexen/textdefs.h Include/jHexen/version.h		       \
Include/jHexen/xddefs.h Include/jHexen/x_config.h		       \
Include/jHexen/m_ctrl.h Include/jHexen/st_stuff.h

EXTRA_DIST = LICENSE apply-style.sh autogen.sh deng-header-style.sh   \
deng-style.sh indent-typenames.sh tags.sh Doc/ChangeLog.txt	      \
Doc/DEDDoc.txt Doc/SrcNotes.txt Doc/Network.txt Doc/jDoom/DoomLic.txt \
Doc/jDoom/JDoom.txt Doc/jHeretic/JHeretic.txt Doc/jHexen/JHexen.txt   \
Doc/Ravenlic.txt Doc/TexC.txt Doc/CVars.txt Build/COMPILING $(Headers)

jDoomDataFiles = Data/jDoom/jDoom.wad Defs/jDoom/Anim.ded	       \
Defs/jDoom/Audio.ded Defs/jDoom/Details.ded Defs/jDoom/Doom1Lights.ded \
Defs/jDoom/Doom2Lights.ded Defs/jDoom/Finales.ded		       \
Defs/jDoom/Lights.ded Defs/jDoom/Maps.ded Defs/jDoom/Objects.ded       \
Defs/jDoom/Particles.ded Defs/jDoom/PlutFinales.ded		       \
Defs/jDoom/Special.ded Defs/jDoom/Sprites.ded			       \
Defs/jDoom/TNTFinales.ded Defs/jDoom/Text.ded Defs/jDoom/Values.ded    \
Defs/jDoom/jDoom.ded Runtime/jDoom/Startup.cfg

jHereticDataFiles = Data/jHeretic/jHeretic.wad Defs/jHeretic/Anim.ded \
Defs/jHeretic/Audio.ded Defs/jHeretic/Finales.ded		      \
Defs/jHeretic/Lights.ded Defs/jHeretic/Maps.ded			      \
Defs/jHeretic/Objects.ded Defs/jHeretic/Special.ded		      \
Defs/jHeretic/Sprites.ded Defs/jHeretic/Text.ded		      \
Defs/jHeretic/jHeretic.ded Runtime/jHeretic/Startup.cfg

jHexenDataFiles = Data/jHexen/jHexen.wad Defs/jHexen/Anim.ded	     \
Defs/jHexen/Audio.ded Defs/jHexen/Finales.ded Defs/jHexen/Lights.ded \
Defs/jHexen/Maps.ded Defs/jHexen/Objects.ded Defs/jHexen/Special.ded \
Defs/jHexen/Sprites.ded Defs/jHexen/Text.ded Defs/jHexen/jHexen.ded  \
Runtime/jHexen/Startup.cfg

nobase_dist_pkgdata_DATA = Data/Doomsday.wad Data/cphelp.txt	   \
Data/KeyMaps/default.dkm Data/KeyMaps/de.dkm Data/KeyMaps/fi.dkm   \
Data/Graphics/Background.pcx Data/Graphics/BoxFill.png		   \
Data/Graphics/Hint.png Data/Graphics/Mouse.png			   \
Data/Graphics/RadioCO.pcx Data/Graphics/BoxCorner.png		   \
Data/Graphics/BoxShade.png Data/Graphics/Logo.png		   \
Data/Graphics/RadioCC.pcx Data/Graphics/WallGlow.pcx		   \
Data/Fonts/Fixed.dfn Data/Fonts/Fixed12.dfn Data/Fonts/Large.dfn   \
Data/Fonts/Small10.dfn Data/Fonts/Small7.dfn Data/Fonts/Small8.dfn \
Data/Fonts/System.dfn Data/Fonts/System12.dfn Defs/Doomsday.ded	   \
Defs/Flags.ded Defs/XG.ded $(jDoomDataFiles) $(jHereticDataFiles)  \
$(jHexenDataFiles)
