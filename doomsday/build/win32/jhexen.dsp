# Microsoft Developer Studio Project File - Name="JHexen" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=JHexen - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "JHexen.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "JHexen.mak" CFG="JHexen - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "JHexen - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "JHexen - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "JHexen - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "./Bin/Release"
# PROP Intermediate_Dir "./Obj/Release/JHexen"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "JHEXEN_EXPORTS" /YX /FD /c
# ADD CPP /nologo /Gz /MT /W3 /GX /O2 /I "Include" /I "Include/JHexen" /I "Include/Common" /D "__JHEXEN__" /D "NORANGECHECKING" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "JHEXEN_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40b /d "NDEBUG"
# ADD RSC /l 0x40b /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 ./bin/release/doomsday.lib lzss.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386

!ELSEIF  "$(CFG)" == "JHexen - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "./Bin/Debug"
# PROP Intermediate_Dir "./Obj/Debug/JHexen"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "JHEXEN_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /Gz /MTd /W3 /Gm /GX /ZI /Od /I "Include" /I "Include/JHexen" /I "Include/Common" /D "__JHEXEN__" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "JHEXEN_EXPORTS" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40b /d "_DEBUG"
# ADD RSC /l 0x40b /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ./bin/debug/doomsday.lib lzss.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "JHexen - Win32 Release"
# Name "JHexen - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "Play"

# PROP Default_Filter "c"
# Begin Source File

SOURCE=.\Src\jHexen\P_acs.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_actor.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_anim.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_ceilng.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_doors.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_enemy.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_floor.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_inter.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_lights.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_map.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_maputl.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_mobj.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_plats.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_pspr.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_setup.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_sight.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_spec.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_start.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_svtexarc.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_switch.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_telept.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_things.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_tick.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\P_user.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_view.c
# End Source File
# End Group
# Begin Group "Base-level"

# PROP Default_Filter "c"
# Begin Source File

SOURCE=.\Src\Common\g_dglinit.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\g_update.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\H2_Actn.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\H2_main.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\HConsole.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\HRefresh.c
# End Source File
# End Group
# Begin Group "Menu"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Src\Common\m_multi.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\Mn_menu.c
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
# Begin Group "XG"

# PROP Default_Filter "c"
# Begin Source File

SOURCE=.\Src\Common\p_xgfile.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_xgline.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_xgsave.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\Src\Common\p_xgsec.c
# PROP Exclude_From_Build 1
# End Source File
# End Group
# Begin Source File

SOURCE=.\Src\jHexen\A_action.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\AcFnLink.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\Am_map.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\Ct_chat.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\f_infine.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\g_game.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\hu_pspr.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\In_lude.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\jhexen.def
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\JHexen.rc
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\M_misc.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\Po_man.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\r_common.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\s_sound.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\Sb_bar.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\Sc_man.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\Sn_sonix.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\Sv_save.c
# End Source File
# Begin Source File

SOURCE=.\Src\jHexen\Tables.c
# End Source File
# Begin Source File

SOURCE=.\Src\Common\x_hair.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "Network Hdrs"

# PROP Default_Filter "h"
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
# Begin Group "XG Hdrs"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\Include\Common\p_xg.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\p_xgline.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\p_xgsec.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\xgclass.h
# End Source File
# End Group
# Begin Group "Doomsday Headers"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=.\Include\dd_api.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_dfdat.h
# End Source File
# Begin Source File

SOURCE=.\Include\dd_share.h
# End Source File
# End Group
# Begin Group "Play Headers"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=.\Include\Common\p_actor.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\P_local.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\p_saveg.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\P_spec.h
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
# End Group
# Begin Source File

SOURCE=.\Include\JHexen\AcFnLink.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\Am_data.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\Am_map.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\Ct_chat.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\f_infine.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\g_common.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\g_demo.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\g_update.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\H2_Actn.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\H2def.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\hu_pspr.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\Info.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\m_bams.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\Mn_def.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\r_common.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\R_local.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\Settings.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\Sounds.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\Soundst.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\Textdefs.h
# End Source File
# Begin Source File

SOURCE=.\Include\Common\x_hair.h
# End Source File
# Begin Source File

SOURCE=.\Include\JHexen\Xddefs.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\Data\JHexen\Hexen.ico
# End Source File
# Begin Source File

SOURCE=.\Data\JHexen\JHexen.ico
# End Source File
# End Group
# End Target
# End Project
