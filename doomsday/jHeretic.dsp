# Microsoft Developer Studio Project File - Name="JHeretic" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=JHeretic - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "JHeretic.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "JHeretic.mak" CFG="JHeretic - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "JHeretic - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "JHeretic - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "JHeretic - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "./Bin/Release"
# PROP Intermediate_Dir "./Obj/Release/JHeretic"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "JHERETIC_EXPORTS" /YX /FD /c
# ADD CPP /nologo /Gz /MT /W3 /GX /O2 /I "./Include/JHeretic" /I "./Include/Common" /I "./Include" /D "__JHERETIC__" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "JHERETIC_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40b /d "NDEBUG"
# ADD RSC /l 0x40b /i "./Include/JHeretic" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 ./bin/release/doomsday.lib lzss.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "JHeretic - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "./Bin/Debug"
# PROP Intermediate_Dir "./Obj/Debug/JHeretic"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "JHERETIC_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /Gz /MTd /W3 /Gm /GX /ZI /Od /I "./Include/JHeretic" /I "./Include/Common" /I "./Include" /D "__JHERETIC__" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "JHERETIC_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40b /d "_DEBUG"
# ADD RSC /l 0x40b /i "./Include/JHeretic" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ./bin/debug/doomsday.lib lzss.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "JHeretic - Win32 Release"
# Name "JHeretic - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "Play"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\Common\p_actor.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_ceilng.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_doors.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_enemy.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_floor.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_inter.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_lights.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_map.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_maputl.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_mobj.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_oldsvg.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_plats.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_pspr.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_saveg.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_setup.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_sight.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\p_sound.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_spec.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_start.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_svtexarc.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_switch.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_telept.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_tick.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\P_user.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_view.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_xgfile.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_xgline.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_xgsave.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_xgsec.c
# End Source File
# End Group
# Begin Group "Base-level"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\Common\g_dglinit.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\H_Action.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\H_Console.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\H_Main.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\H_Refresh.c
# End Source File
# End Group
# Begin Group "Menu"

# PROP Default_Filter "c"
# Begin Source File

SOURCE=.\Src\Common\m_multi.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\Mn_menu.c
# End Source File
# End Group
# Begin Group "Network"

# PROP Default_Filter "c"
# Begin Source File

SOURCE=.\Src\Common\d_Net.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\d_NetCl.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\d_NetSv.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\Src\jHeretic\AcFnLink.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\Am_map.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\Ct_chat.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\f_infine.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\g_game.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\g_update.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\hu_pspr.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\In_lude.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\jheretic.def
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\M_misc.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\r_common.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\Sb_bar.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\Tables.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\V_video.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\x_hair.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "Doomsday"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Include\dd_api.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_dfdat.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_share.h
# End Source File
# Begin Source File

SOURCE=.\Include\doomsday.h
# End Source File
# End Group
# Begin Group "Net headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Include\Common\d_Net.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\d_NetCl.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\d_NetSv.h
# End Source File
# End Group
# Begin Group "Play headers"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=.\Include\Common\p_actor.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\P_local.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\p_oldsvg.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\p_saveg.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\P_spec.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\p_start.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\p_svtexarc.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\p_view.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\p_xg.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\p_xgline.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\p_xgsec.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Include\JHeretic\AcFnLink.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\Am_data.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\Am_map.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\Ct_chat.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\Doomdata.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\Doomdef.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\Drcoord.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\Dstrings.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\f_infine.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\g_common.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\g_dgl.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\g_update.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\H_Action.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\hu_pspr.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\I_header.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\I_sound.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\Info.h
# End Source File
# Begin Source File

SOURCE=.\Include\m_bams.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\Mn_def.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\r_common.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\R_local.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\resource.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\s_common.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\S_sound.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\settings.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\Sounds.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\Soundst.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHeretic\Vgaview.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\x_hair.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\xgclass.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\Data\JHeretic\heretic.ico
# End Source File
# Begin Source File

SOURCE=.\Data\JHeretic\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\Src\jHeretic\JHeretic.rc
# End Source File
# End Group
# End Target
# End Project
