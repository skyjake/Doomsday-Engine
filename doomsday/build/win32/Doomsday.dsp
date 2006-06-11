# Microsoft Developer Studio Project File - Name="Doomsday" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=Doomsday - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Doomsday.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Doomsday.mak" CFG="Doomsday - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Doomsday - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "Doomsday - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Doomsday - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "./Bin/Release"
# PROP Intermediate_Dir "./Obj/Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /Gz /MT /W3 /GX /Z7 /Ot /Oi /Oy /Ob1 /Gf /Gy /I "./Include" /I "./Include/jtNet2" /D "__DOOMSDAY__" /D "WIN32_GAMMA" /D "NORANGECHECKING" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /Gs /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40b /d "NDEBUG"
# ADD RSC /l 0x40b /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 wsock32.lib libpng.lib libz.lib fmodvc.lib lzss.lib dinput.lib dsound.lib eaxguid.lib dxguid.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386 /libpath:"./Bin/Release"
# SUBTRACT LINK32 /profile /incremental:yes /debug

!ELSEIF  "$(CFG)" == "Doomsday - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "./Bin/Debug"
# PROP Intermediate_Dir "./Obj/Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /Gz /MTd /W3 /Gm /GX /ZI /Od /I "./Include" /I "./Include/jtNet2" /D "__DOOMSDAY__" /D "WIN32_GAMMA" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40b /d "_DEBUG"
# ADD RSC /l 0x40b /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 wsock32.lib libz.lib libpngd.lib fmodvc.lib dsound.lib dinput.lib dxguid.lib lzss.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /version:1.0 /subsystem:windows /debug /machine:I386 /libpath:"./Bin/Debug"
# SUBTRACT LINK32 /profile /nodefaultlib

!ENDIF 

# Begin Target

# Name "Doomsday - Win32 Release"
# Name "Doomsday - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "Base"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\dd_dgl.c
# End Source File
# Begin Source File

SOURCE=.\Src\dd_help.c
# End Source File
# Begin Source File

SOURCE=.\Src\dd_input.c
# End Source File
# Begin Source File

SOURCE=.\Src\dd_loop.c
# End Source File
# Begin Source File

SOURCE=.\Src\dd_main.c
# End Source File
# Begin Source File

SOURCE=.\Src\dd_plugin.c
# End Source File
# Begin Source File

SOURCE=.\Src\dd_wad.c
# End Source File
# Begin Source File

SOURCE=.\Src\dd_winit.c
# End Source File
# Begin Source File

SOURCE=.\Src\dd_zip.c
# End Source File
# Begin Source File

SOURCE=.\Src\dd_zone.c
# End Source File
# End Group
# Begin Group "Console"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\con_action.c
# End Source File
# Begin Source File

SOURCE=.\Src\con_bar.c
# End Source File
# Begin Source File

SOURCE=.\Src\con_bind.c
# End Source File
# Begin Source File

SOURCE=.\Src\con_config.c
# End Source File
# Begin Source File

SOURCE=.\Src\con_main.c
# End Source File
# Begin Source File

SOURCE=.\Src\con_start.c
# End Source File
# End Group
# Begin Group "System"

# PROP Default_Filter ""
# Begin Group "Audio Drivers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\sys_musd_fmod.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_musd_win.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_sfxd_ds.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_sfxd_dummy.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_sfxd_loader.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\Src\sys_console.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_direc.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_filein.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_input.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_master.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_mixer.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_network.cpp
# End Source File
# Begin Source File

SOURCE=.\Src\sys_sock.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_stwin.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_system.c
# End Source File
# Begin Source File

SOURCE=.\Src\sys_timer.c
# End Source File
# End Group
# Begin Group "Network"

# PROP Default_Filter ""
# Begin Group "Client"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\cl_frame.c
# End Source File
# Begin Source File

SOURCE=.\Src\cl_main.c
# End Source File
# Begin Source File

SOURCE=.\Src\cl_mobj.c
# End Source File
# Begin Source File

SOURCE=.\Src\cl_player.c
# End Source File
# Begin Source File

SOURCE=.\Src\cl_sound.c
# End Source File
# Begin Source File

SOURCE=.\Src\cl_world.c
# End Source File
# End Group
# Begin Group "Server"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\sv_frame.c
# End Source File
# Begin Source File

SOURCE=.\Src\sv_main.c
# End Source File
# Begin Source File

SOURCE=.\Src\sv_missile.c
# End Source File
# Begin Source File

SOURCE=.\Src\sv_pool.c
# End Source File
# Begin Source File

SOURCE=.\Src\sv_sound.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\Src\net_demo.c
# End Source File
# Begin Source File

SOURCE=.\Src\net_main.c
# End Source File
# Begin Source File

SOURCE=.\Src\net_msg.c
# End Source File
# Begin Source File

SOURCE=.\Src\net_ping.c
# End Source File
# End Group
# Begin Group "Play"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\p_cmd.c
# End Source File
# Begin Source File

SOURCE=.\Src\p_data.c
# End Source File
# Begin Source File

SOURCE=.\Src\p_intercept.c
# End Source File
# Begin Source File

SOURCE=.\Src\p_maputil.c
# End Source File
# Begin Source File

SOURCE=.\Src\p_mobj.c
# End Source File
# Begin Source File

SOURCE=.\Src\p_particle.c
# End Source File
# Begin Source File

SOURCE=.\Src\p_player.c
# End Source File
# Begin Source File

SOURCE=.\Src\p_polyob.c
# End Source File
# Begin Source File

SOURCE=.\Src\p_sight.c
# End Source File
# Begin Source File

SOURCE=.\Src\p_think.c
# End Source File
# Begin Source File

SOURCE=.\Src\p_tick.c
# End Source File
# End Group
# Begin Group "Refresh"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\r_data.c
# End Source File
# Begin Source File

SOURCE=.\Src\r_draw.c
# End Source File
# Begin Source File

SOURCE=.\Src\r_extres.c
# End Source File
# Begin Source File

SOURCE=.\Src\r_main.c
# End Source File
# Begin Source File

SOURCE=.\Src\r_model.c
# End Source File
# Begin Source File

SOURCE=.\Src\r_sky.c
# End Source File
# Begin Source File

SOURCE=.\Src\r_things.c
# End Source File
# Begin Source File

SOURCE=.\Src\r_util.c
# End Source File
# Begin Source File

SOURCE=.\Src\r_world.c
# End Source File
# End Group
# Begin Group "Renderer"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\rend_clip.c
# End Source File
# Begin Source File

SOURCE=.\Src\rend_decor.c
# End Source File
# Begin Source File

SOURCE=.\Src\rend_dyn.c
# End Source File
# Begin Source File

SOURCE=.\Src\rend_halo.c
# End Source File
# Begin Source File

SOURCE=.\Src\rend_list.c
# End Source File
# Begin Source File

SOURCE=.\Src\rend_main.c
# End Source File
# Begin Source File

SOURCE=.\Src\rend_model.c
# End Source File
# Begin Source File

SOURCE=.\Src\rend_particle.c
# End Source File
# Begin Source File

SOURCE=.\Src\rend_shadow.c
# End Source File
# Begin Source File

SOURCE=.\Src\rend_sky.c
# End Source File
# Begin Source File

SOURCE=.\Src\rend_sprite.c
# End Source File
# End Group
# Begin Group "Graphics"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\gl_draw.c
# End Source File
# Begin Source File

SOURCE=.\Src\gl_font.c
# End Source File
# Begin Source File

SOURCE=.\Src\gl_main.c
# End Source File
# Begin Source File

SOURCE=.\Src\gl_pcx.c
# End Source File
# Begin Source File

SOURCE=.\Src\gl_png.c
# End Source File
# Begin Source File

SOURCE=.\Src\gl_tex.c
# End Source File
# Begin Source File

SOURCE=.\Src\gl_tga.c
# End Source File
# End Group
# Begin Group "Audio"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\s_cache.c
# End Source File
# Begin Source File

SOURCE=.\Src\s_environ.c
# End Source File
# Begin Source File

SOURCE=.\Src\s_logic.c
# End Source File
# Begin Source File

SOURCE=.\Src\s_main.c
# End Source File
# Begin Source File

SOURCE=.\Src\s_mus.c
# End Source File
# Begin Source File

SOURCE=.\Src\s_sfx.c
# End Source File
# Begin Source File

SOURCE=.\Src\s_wav.c
# End Source File
# End Group
# Begin Group "Miscellaneous"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\m_args.c
# End Source File
# Begin Source File

SOURCE=.\Src\m_bams.c
# End Source File
# Begin Source File

SOURCE=.\Src\m_filehash.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\m_fixed.c
# End Source File
# Begin Source File

SOURCE=.\Src\m_huffman.c
# End Source File
# Begin Source File

SOURCE=.\Src\m_misc.c
# End Source File
# Begin Source File

SOURCE=.\Src\m_nodepile.c
# End Source File
# Begin Source File

SOURCE=.\Src\m_string.c
# End Source File
# End Group
# Begin Group "Tables"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\tab_tables.c
# End Source File
# Begin Source File

SOURCE=.\Src\tab_video.c
# End Source File
# End Group
# Begin Group "UI"

# PROP Default_Filter ".c"
# Begin Source File

SOURCE=.\Src\ui_main.c
# End Source File
# Begin Source File

SOURCE=.\Src\ui_mpi.c
# End Source File
# Begin Source File

SOURCE=.\Src\ui_panel.c
# End Source File
# End Group
# Begin Group "Definitions"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\def_data.c
# End Source File
# Begin Source File

SOURCE=.\Src\def_main.c
# End Source File
# Begin Source File

SOURCE=.\Src\def_read.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\Src\Doomsday.def
# End Source File
# Begin Source File

SOURCE=.\Src\template.c

!IF  "$(CFG)" == "Doomsday - Win32 Release"

!ELSEIF  "$(CFG)" == "Doomsday - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "Base Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\dd_api.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_def.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_dgl.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_help.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_input.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_loop.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_main.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_plugin.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_share.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_types.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_wad.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_winit.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_zip.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_zone.h
# End Source File
# Begin Source File

SOURCE=.\Include\de_base.h
# End Source File
# End Group
# Begin Group "Console Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\con_action.h
# End Source File
# Begin Source File

SOURCE=.\Include\con_bar.h
# End Source File
# Begin Source File

SOURCE=.\Include\con_bind.h
# End Source File
# Begin Source File

SOURCE=.\Include\con_config.h
# End Source File
# Begin Source File

SOURCE=.\Include\con_decl.h
# End Source File
# Begin Source File

SOURCE=.\Include\con_main.h
# End Source File
# Begin Source File

SOURCE=.\Include\con_start.h
# End Source File
# Begin Source File

SOURCE=.\Include\de_console.h
# End Source File
# End Group
# Begin Group "System Headers"

# PROP Default_Filter ""
# Begin Group "Audio Driver Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\sys_audio.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_musd.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_musd_fmod.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_musd_win.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_sfxd.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_sfxd_ds.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_sfxd_dummy.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_sfxd_loader.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Include\de_system.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_console.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_direc.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_file.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_input.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_master.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_network.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_sock.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_stwin.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_system.h
# End Source File
# Begin Source File

SOURCE=.\Include\sys_timer.h
# End Source File
# End Group
# Begin Group "Network Headers"

# PROP Default_Filter ""
# Begin Group "Client Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\cl_def.h
# End Source File
# Begin Source File

SOURCE=.\Include\cl_frame.h
# End Source File
# Begin Source File

SOURCE=.\Include\cl_mobj.h
# End Source File
# Begin Source File

SOURCE=.\Include\cl_player.h
# End Source File
# Begin Source File

SOURCE=.\Include\cl_sound.h
# End Source File
# Begin Source File

SOURCE=.\Include\cl_world.h
# End Source File
# End Group
# Begin Group "Server Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\sv_def.h
# End Source File
# Begin Source File

SOURCE=.\Include\sv_frame.h
# End Source File
# Begin Source File

SOURCE=.\Include\sv_missile.h
# End Source File
# Begin Source File

SOURCE=.\Include\sv_pool.h
# End Source File
# Begin Source File

SOURCE=.\Include\sv_sound.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Include\de_network.h
# End Source File
# Begin Source File

SOURCE=.\Include\net_demo.h
# End Source File
# Begin Source File

SOURCE=.\Include\net_main.h
# End Source File
# Begin Source File

SOURCE=.\Include\net_msg.h
# End Source File
# End Group
# Begin Group "Play Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\de_play.h
# End Source File
# Begin Source File

SOURCE=.\Include\p_data.h
# End Source File
# Begin Source File

SOURCE=.\Include\p_intercept.h
# End Source File
# Begin Source File

SOURCE=.\Include\p_maputil.h
# End Source File
# Begin Source File

SOURCE=.\Include\p_mobj.h
# End Source File
# Begin Source File

SOURCE=.\Include\p_particle.h
# End Source File
# Begin Source File

SOURCE=.\Include\p_player.h
# End Source File
# Begin Source File

SOURCE=.\Include\p_polyob.h
# End Source File
# Begin Source File

SOURCE=.\Include\p_sight.h
# End Source File
# Begin Source File

SOURCE=.\Include\p_think.h
# End Source File
# Begin Source File

SOURCE=.\Include\p_tick.h
# End Source File
# End Group
# Begin Group "Refresh Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\de_refresh.h
# End Source File
# Begin Source File

SOURCE=.\Include\r_data.h
# End Source File
# Begin Source File

SOURCE=.\Include\r_draw.h
# End Source File
# Begin Source File

SOURCE=.\Include\r_extres.h
# End Source File
# Begin Source File

SOURCE=.\Include\r_main.h
# End Source File
# Begin Source File

SOURCE=.\Include\r_model.h
# End Source File
# Begin Source File

SOURCE=.\Include\r_sky.h
# End Source File
# Begin Source File

SOURCE=.\Include\r_things.h
# End Source File
# Begin Source File

SOURCE=.\Include\r_util.h
# End Source File
# Begin Source File

SOURCE=.\Include\r_world.h
# End Source File
# End Group
# Begin Group "Renderer Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\de_render.h
# End Source File
# Begin Source File

SOURCE=.\Include\rend_clip.h
# End Source File
# Begin Source File

SOURCE=.\Include\rend_decor.h
# End Source File
# Begin Source File

SOURCE=.\Include\rend_dyn.h
# End Source File
# Begin Source File

SOURCE=.\Include\rend_halo.h
# End Source File
# Begin Source File

SOURCE=.\Include\rend_list.h
# End Source File
# Begin Source File

SOURCE=.\Include\rend_main.h
# End Source File
# Begin Source File

SOURCE=.\Include\rend_model.h
# End Source File
# Begin Source File

SOURCE=.\Include\rend_particle.h
# End Source File
# Begin Source File

SOURCE=.\Include\rend_shadow.h
# End Source File
# Begin Source File

SOURCE=.\Include\rend_sky.h
# End Source File
# Begin Source File

SOURCE=.\Include\rend_sprite.h
# End Source File
# End Group
# Begin Group "Graphics Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\de_graphics.h
# End Source File
# Begin Source File

SOURCE=.\Include\dglib.h
# End Source File
# Begin Source File

SOURCE=.\Include\gl_draw.h
# End Source File
# Begin Source File

SOURCE=.\Include\gl_font.h
# End Source File
# Begin Source File

SOURCE=.\Include\gl_main.h
# End Source File
# Begin Source File

SOURCE=.\Include\gl_model.h
# End Source File
# Begin Source File

SOURCE=.\Include\gl_pcx.h
# End Source File
# Begin Source File

SOURCE=.\Include\gl_png.h
# End Source File
# Begin Source File

SOURCE=.\Include\gl_tex.h
# End Source File
# Begin Source File

SOURCE=.\Include\gl_tga.h
# End Source File
# End Group
# Begin Group "Audio Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\de_audio.h
# End Source File
# Begin Source File

SOURCE=.\Include\s_cache.h
# End Source File
# Begin Source File

SOURCE=.\Include\s_environ.h
# End Source File
# Begin Source File

SOURCE=.\Include\s_logic.h
# End Source File
# Begin Source File

SOURCE=.\Include\s_main.h
# End Source File
# Begin Source File

SOURCE=.\Include\s_mus.h
# End Source File
# Begin Source File

SOURCE=.\Include\s_sfx.h
# End Source File
# Begin Source File

SOURCE=.\Include\s_wav.h
# End Source File
# End Group
# Begin Group "Misc Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\de_misc.h
# End Source File
# Begin Source File

SOURCE=.\Include\m_args.h
# End Source File
# Begin Source File

SOURCE=.\Include\m_bams.h
# End Source File
# Begin Source File

SOURCE=.\Include\m_filehash.h
# End Source File
# Begin Source File

SOURCE=.\Include\m_huffman.h
# End Source File
# Begin Source File

SOURCE=.\Include\m_misc.h
# End Source File
# Begin Source File

SOURCE=.\Include\m_nodepile.h
# End Source File
# Begin Source File

SOURCE=.\Include\m_profiler.h
# End Source File
# Begin Source File

SOURCE=.\Include\m_string.h
# End Source File
# End Group
# Begin Group "Table Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\tab_anorms.h
# End Source File
# End Group
# Begin Group "UI Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\de_ui.h
# End Source File
# Begin Source File

SOURCE=.\Include\ui_main.h
# End Source File
# Begin Source File

SOURCE=.\Include\ui_mpi.h
# End Source File
# Begin Source File

SOURCE=.\Include\ui_panel.h
# End Source File
# End Group
# Begin Group "Definition Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\de_defs.h
# End Source File
# Begin Source File

SOURCE=.\Include\def_data.h
# End Source File
# Begin Source File

SOURCE=.\Include\def_main.h
# End Source File
# Begin Source File

SOURCE=.\Include\def_share.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Include\doomsday.h

!IF  "$(CFG)" == "Doomsday - Win32 Release"

!ELSEIF  "$(CFG)" == "Doomsday - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Include\LZSS.h
# End Source File
# Begin Source File

SOURCE=.\Include\template.h

!IF  "$(CFG)" == "Doomsday - Win32 Release"

!ELSEIF  "$(CFG)" == "Doomsday - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\Src\Doomsday.rc
# End Source File
# Begin Source File

SOURCE=.\Res\icon2.ico
# End Source File
# Begin Source File

SOURCE=.\res\icon3.ico
# End Source File
# End Group
# Begin Source File

SOURCE=.\Doc\Banner.txt
# End Source File
# End Target
# End Project
