@ECHO OFF
IF "%1" == "" GOTO Help
IF "%1" == "help" GOTO Help
GOTO SetPaths
:Help
cls
ECHO This build script compiles the Doomsday Engine and the associated
ECHO libraries.
ECHO.
ECHO Microsoft Visual C++ Toolkit is the minimum requirement.  If you
ECHO want to compile the engine itself, you will also need the Platform
ECHO SDK, which can be downloaded from Microsoft's website.  The DirectX
ECHO SDK is also required for compiling the engine.  This means you can
ECHO compile everything using tools that can be downloaded for free, but
ECHO all the SDKs will add up to several hundred megabytes.
ECHO.
ECHO On the bright side, you only need the Visual C++ Toolkit in order to
ECHO compile a game DLL.  The game DLLs' only dependency is Doomsday.lib,
ECHO which is included in the Doomsday source code package.
ECHO.
IF EXIST vcconfig.bat GOTO FoundConfig

:MissingConfig
ECHO IMPORTANT:
ECHO In order to use this batch file you'll first need to configure the
ECHO include paths to any dependancies of the code you wish to compile.
ECHO For example if you want to compile Doomsday.exe (Engine) you'll need
ECHO to set up the path to your Platform SDK (amongst others).
ECHO.
ECHO Open the file "vcconfig-example.bat", edit the paths appropriately
ECHO and then save your changes into a new file called:
ECHO.
ECHO   vcconfig.bat
ECHO.
GOTO TheRealEnd

:FoundConfig
ECHO Throughout this batch file there are targets that can be given as
ECHO arguments on the command line.  For example, to compile jDoom you
ECHO would use the "jdoom" target:
ECHO.
ECHO   vcbuild jdoom
ECHO.
ECHO The "all" target compiles everything in the appropriate order:
ECHO.
ECHO   vcbuild all
ECHO.
ECHO Build Tip:
ECHO To output the messages produced during compilation to a log file,
ECHO append the build target with the redirection character ">" followed
ECHO by a filename:
ECHO.
ECHO   vcbuild doomsday^>buildlog.txt
GOTO TheRealEnd

:SetPaths
:: -- Set up paths.
IF NOT EXIST vcconfig.bat GOTO MissingConfig

CALL vcconfig.bat
SET SOMETHING_FAILED=0

SET DENG_ENGINE_DIR=.\..\..\engine
SET DENG_ENGINE_API_DIR=.\..\..\engine\api
SET DENG_PLUGINS_DIR=.\..\..\plugins
SET DENG_EXTERNAL_DIR=.\..\..\external

:: -- Make sure the output directories exist.
IF NOT EXIST %RELEASE_BIN_DIR% md %RELEASE_BIN_DIR%
IF NOT EXIST %RELEASE_OBJ_DIR% md %RELEASE_OBJ_DIR%
IF NOT EXIST %DEBUG_BIN_DIR% md %DEBUG_BIN_DIR%
IF NOT EXIST %DEBUG_OBJ_DIR% md %DEBUG_OBJ_DIR%

:: -- Compiler and linker options. Defaults to a Release build.
SET BUILDMODE=
SET RELEASEDEFS=/MT /D "NORANGECHECKING" /D "NDEBUG" 
SET DEBUGDEFS=/Z7 /MTd /D "_DEBUG"
SET BUILDDEFS=%RELEASEDEFS%
SET OBJ_DIR=%RELEASE_OBJ_DIR%
SET BIN_DIR=%RELEASE_BIN_DIR%
SET LIBS=/libpath:".\lib" /libpath:"%SDL_LIB%" /libpath:"%SDLNET_LIB%" /libpath:"%SDLMIXER_LIB%" /libpath:"%BIN_DIR%"
SET DEFINES=/D "ZLIB_DLL" /D "WIN32_GAMMA" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_CRT_SECURE_NO_DEPRECATE" /DDOOMSDAY_BUILD_TEXT="\"%DOOMSDAY_BUILD%\""
SET DLLDEFINES=/D "_USRDLL" /D "_WINDLL" %DEFINES%

SET INCS_ENGINE_API=%DENG_ENGINE_API_DIR%
SET INCS_ENGINE_PORTABLE=%DENG_ENGINE_DIR%\portable\include
SET INCS_ENGINE_WIN32=%DENG_ENGINE_DIR%\win32\include
SET INCS_LZSS_PORTABLE=%DENG_EXTERNAL_DIR%\lzss\portable\include
SET INCS_LIBPNG_PORTABLE=%DENG_EXTERNAL_DIR%\libpng\portable\include
SET INCS_ZLIB=%DENG_EXTERNAL_DIR%\zlib\portable\include
SET INCS_LIBCURL=%DENG_EXTERNAL_DIR%\libcurl\portable\include
SET INCS_PLUGIN_COMMON=%DENG_PLUGINS_DIR%\common\include

SET LFLAGS=/incremental:NO /subsystem:WINDOWS /machine:I386


:: -- Iterate through all command line arguments.
:Looper
IF "%1" == "" GOTO TheEnd
GOTO %1


:: *** Build options
:Debug
ECHO Debug build mode selected.
SET BUILDMODE=debug
SET BUILDDEFS=%DEBUGDEFS%
SET OBJ_DIR=%DEBUG_OBJ_DIR%
SET BIN_DIR=%DEBUG_BIN_DIR%
SET LFLAGS=%LFLAGS% /NODEFAULTLIB:libcmt 
SET LIBS=/libpath:".\lib" /libpath:"%SDL_LIB%" /libpath:"%SDLNET_LIB%" /libpath:"%SDLMIXER_LIB%" /libpath:"%BIN_DIR%"
GOTO Done
:_P
SET PROFILEDEFINES=/D "DD_PROFILE"
GOTO Done

:: *** Cleanup and build all targets.
:All
CALL vcbuild.bat %BUILDMODE% cleanup res dmt doomsday dpdehread dpwadmapconverter dsopenal dswinmm dsdirectsound jdoom jheretic jhexen jdoom64 copydep
GOTO Done


:: *** Clean the output directories.
:Cleanup
rd/s/q %BIN_DIR%
rd/s/q %OBJ_DIR%
md %BIN_DIR%
md %OBJ_DIR%
GOTO Done


:: *** Resources
:: Requires rc.exe and cvtres.exe from the Platform SDK.
:Res
ECHO ***************************************************************************
ECHO ****************** Generating resources from rc scripts *******************
ECHO ***************************************************************************
ECHO Processing doomsday.rc...
rc /i "%PLATFORM_INC%" /i "%PLATFORM_INC%\mfc" "%DENG_ENGINE_DIR%\win32\res\doomsday.rc"
ECHO Processing jdoom.rc...
rc /D "__JDOOM__" /i "%PLATFORM_INC%" /i "%PLATFORM_INC%\mfc" "%DENG_PLUGINS_DIR%\jdoom\res\jdoom.rc"
ECHO Processing jdoom64.rc...
rc /D "__JDOOM64__" /i "%PLATFORM_INC%" /i "%PLATFORM_INC%\mfc" "%DENG_PLUGINS_DIR%\jdoom64\res\jdoom64.rc"
ECHO Processing jheretic.rc...
rc /D "__JHERETIC__" /i "%PLATFORM_INC%" /i "%PLATFORM_INC%\mfc" "%DENG_PLUGINS_DIR%\jheretic\res\jheretic.rc"
ECHO Processing jhexen.rc...
rc /D "__JHEXEN__" /i "%PLATFORM_INC%" /i "%PLATFORM_INC%\mfc" "%DENG_PLUGINS_DIR%\jhexen\res\jhexen.rc"
ECHO Processing dehread.rc...
rc /i "%PLATFORM_INC%" /i "%PLATFORM_INC%\mfc" "%DENG_PLUGINS_DIR%\dehread\res\dehread.rc"
ECHO Processing directsound.rc...
rc /i "%PLATFORM_INC%" /i "%PLATFORM_INC%\mfc" "%DENG_PLUGINS_DIR%\directsound\res\directsound.rc"
ECHO Processing openal.rc...
rc /i "%PLATFORM_INC%" /i "%PLATFORM_INC%\mfc" "%DENG_PLUGINS_DIR%\openal\res\openal.rc"
ECHO Processing wadmapconverter.rc...
rc /i "%PLATFORM_INC%" /i "%PLATFORM_INC%\mfc" "%DENG_PLUGINS_DIR%\wadmapconverter\res\wadmapconverter.rc"
ECHO Processing winmm.rc...
rc /i "%PLATFORM_INC%" /i "%PLATFORM_INC%\mfc" "%DENG_PLUGINS_DIR%\winmm\res\winmm.rc"
ECHO Processing example.rc...
rc /D "__EXAMPLE_PLUGIN__" /i "%PLATFORM_INC%" /i "%PLATFORM_INC%\mfc" "%DENG_PLUGINS_DIR%\exampleplugin\res\example.rc"


ECHO ***************************************************************************
ECHO ********************* Converting resources to objects *********************
ECHO ***************************************************************************
ECHO Processing Doomsday.res...
cvtres /out:"%OBJ_DIR%\doomsday_res.obj" /MACHINE:X86 "%DENG_ENGINE_DIR%\win32\res\doomsday.res"
ECHO Processing jDoom.res...
md %OBJ_DIR%\jdoom
cvtres /out:"%OBJ_DIR%\jdoom\jdoom_res.obj" /MACHINE:X86 "%DENG_PLUGINS_DIR%\jdoom\res\jdoom.res"
ECHO Processing jDoom64.res...
md %OBJ_DIR%\jdoom64
cvtres /out:"%OBJ_DIR%\jdoom64\jdoom64_res.obj" /MACHINE:X86 "%DENG_PLUGINS_DIR%\jdoom64\res\jdoom64.res"
ECHO Processing jHeretic.res...
md %OBJ_DIR%\jheretic
cvtres /out:"%OBJ_DIR%\jheretic\jheretic_res.obj" /MACHINE:X86 "%DENG_PLUGINS_DIR%\jheretic\res\jheretic.res"
ECHO Processing jHexen.res...
md %OBJ_DIR%\jhexen
cvtres /out:"%OBJ_DIR%\jhexen\jhexen_res.obj" /MACHINE:X86 "%DENG_PLUGINS_DIR%\jhexen\res\jhexen.res"
ECHO Processing dehread.res...
md %OBJ_DIR%\dpdehread
cvtres /out:"%OBJ_DIR%\dpdehread\dpdehread_res.obj" /MACHINE:X86 "%DENG_PLUGINS_DIR%\dehread\res\dehread.res"
ECHO Processing directsound.res...
md %OBJ_DIR%\dsdirectsound
cvtres /out:"%OBJ_DIR%\dsdirectsound\dsdirectsound_res.obj" /MACHINE:X86 "%DENG_PLUGINS_DIR%\directsound\res\directsound.res"
ECHO Processing openal.res...
md %OBJ_DIR%\dsopenal
cvtres /out:"%OBJ_DIR%\dsopenal\dsopenal_res.obj" /MACHINE:X86 "%DENG_PLUGINS_DIR%\openal\res\openal.res"
ECHO Processing wadmapconverter.res...
md %OBJ_DIR%\dpwadmapconverter
cvtres /out:"%OBJ_DIR%\dpwadmapconverter\dpwadmapconverter_res.obj" /MACHINE:X86 "%DENG_PLUGINS_DIR%\wadmapconverter\res\wadmapconverter.res"
ECHO Processing winmm.res...
md %OBJ_DIR%\dswinmm
cvtres /out:"%OBJ_DIR%\dswinmm\dswinmm_res.obj" /MACHINE:X86 "%DENG_PLUGINS_DIR%\winmm\res\winmm.res"
ECHO Processing example.res...
md %OBJ_DIR%\dpexample
cvtres /out:"%OBJ_DIR%\exampleplugin\dpexample_res.obj" /MACHINE:X86 "%DENG_PLUGINS_DIR%\exampleplugin\res\example.res"


IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** Execute a Python Script
:: Requires Python interpreter.
:Script
ECHO Run Script: %2
SET CURRENTSCRIPT=%2
"%PYTHON_DIR%"\python "%CURRENTSCRIPT%"
SHIFT
GOTO Done


:: *** Copy various files to the build directory, for packaging.
:copydep
copy "%SDL_LIB%\SDL.dll" .
copy "%SDLNET_LIB%\SDL_net.dll" .
copy "%SDLMIXER_LIB%\libogg-0.dll" .
copy "%SDLMIXER_LIB%\libvorbis-0.dll" .
copy "%SDLMIXER_LIB%\libvorbisfile-3.dll" .
copy "%SDLMIXER_LIB%\mikmod.dll" .
copy "%SDLMIXER_LIB%\SDL_mixer.dll" .
copy "%SDLMIXER_LIB%\smpeg.dll" .
copy "%EAX_DLL%\eax.dll" .
copy "%DENG_ENGINE_DIR%\doc\LICENSE" license.txt
GOTO Done

:: *** Mapdata type headers
:CheckDMT
:: First check for their existence
IF NOT EXIST "%DENG_ENGINE_API_DIR%\dd_maptypes.h" GOTO DMT
IF NOT EXIST "%DENG_ENGINE_DIR%\include\p_maptypes.h" GOTO DMT
:: TODO compare creation date&time with that of mapdata.hs
:: If mapdata.hs is newer - recreate the headers.
ECHO  DMT headers are up to date.
GOTO TheRealEnd

:DMT
SET SCRIPTPARAMATERS="%DENG_ENGINE_DIR%\scripts\makedmt.py < %DENG_ENGINE_DIR%\portable\include\mapdata.hs"
CALL vcbuild.bat %BUILDMODE% script %SCRIPTPARAMATERS%
MOVE /Y dd_maptypes.h "%DENG_ENGINE_API_DIR%\\"
MOVE /Y p_maptypes.h "%DENG_ENGINE_DIR%\portable\include\\"
GOTO DONE


:: *** Doomsday.exe
:Doomsday
CALL vcbuild.bat %BUILDMODE% checkdmt
ECHO ***************************************************************************
ECHO ********************   Compiling Doomsday.exe (Engine)   ******************
ECHO ***************************************************************************
cl /Ob1 /Oi /Ot /Oy /GF /FD /EHsc /GS /Gy /Fo"%OBJ_DIR%\\" /Fd"%OBJ_DIR%\\" /W3 /Gd /Gs /I "%INCS_ENGINE_API%\\" /I "%EAX_INC%" /I "%SDL_INC%" /I "%SDLMIXER_INC%" /I "%SDLNET_INC%" /I "%DX_INC%" /I "%PLATFORM_INC%" /I "%INCS_ENGINE_WIN32%\\" /I "%GL_INC%" /I "%INCS_ENGINE_PORTABLE%\\" /I "%INCS_LZSS_PORTABLE%\\" /I "%INCS_LIBPNG_PORTABLE%\\" /I "%INCS_LIBCURL%\\" /I "%INCS_ZLIB%\\" /I "%INCS_PLUGIN_COMMON%\\" %DEFINES% %BUILDDEFS% %PROFILEDEFINES% /D "__DOOMSDAY__"  "%OBJ_DIR%\doomsday_res.obj"  @doomsday_cl.rsp  /link /out:"%BIN_DIR%\Doomsday.exe" /def:"%DENG_ENGINE_API_DIR%\doomsday.def" /implib:"%BIN_DIR%\Doomsday.lib" %LFLAGS% %LIBS% /libpath:"%PLATFORM_LIB%\\" /libpath:"%DX_LIB%\\" "%DENG_EXTERNAL_DIR%\libpng\win32\libpng13.lib" "%DENG_EXTERNAL_DIR%\zlib\win32\zlib1.lib" "%DENG_EXTERNAL_DIR%\lzss\win32\lzss.lib" "%DENG_EXTERNAL_DIR%\libcurl\win32\curllib.lib" sdl_net.lib sdl.lib SDL_mixer.lib wsock32.lib dinput8.lib dsound.lib dxguid.lib winmm.lib opengl32.lib glu32.lib kernel32.lib gdi32.lib ole32.lib user32.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** dpDehRead.dll
:dpDehRead
ECHO ***************************************************************************
ECHO ***********   Compiling dpDehRead.dll (Dehacked Reader Plugin)   **********
ECHO ***************************************************************************
md %OBJ_DIR%\dpDehRead
cl /O2 /Ob1 /I "%INCS_ENGINE_API%\\" %BUILDDEFS% %DLLDEFINES% /D "DPDEHREAD_EXPORTS" /GF /FD /EHsc /Gy /Fo"%OBJ_DIR%\dpDehRead\\" /Fd"%OBJ_DIR%\dpDehRead\\" /W3 /Gd "%OBJ_DIR%\dpdehread\dpdehread_res.obj" @dpdehread_cl.rsp   /link /out:"%BIN_DIR%\dpDehRead.dll" %LFLAGS% /dll /def:"%DENG_PLUGINS_DIR%\dehread\api\dpdehread.def" /implib:"%BIN_DIR%\dpDehRead.lib" %LIBS% %BIN_DIR%\Doomsday.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** dsOpenAL.dll
:dsOpenAL
ECHO ***************************************************************************
ECHO ************   Compiling dsOpenAL.dll (OpenAL SoundFX driver)   ***********
ECHO ***************************************************************************
md %OBJ_DIR%\dsOpenAL
cl /O2 /Ob1 /I "%INCS_ENGINE_API%\\" %BUILDDEFS% %DLLDEFINES% /D "DSOPENAL_EXPORTS" /I "%OPENAL_INC%" /GF /FD /EHsc /Gy /Fo"%OBJ_DIR%\dsOpenAL" /Fd"%OBJ_DIR%\dsOpenAL" /W3 /Gd "%OBJ_DIR%\dsopenal\dsopenal_res.obj" @dsopenal_cl.rsp  /link /out:"%BIN_DIR%\dsOpenAL.dll" %LFLAGS% /dll /def:"%DENG_PLUGINS_DIR%\openal\api\dsOpenAL.def" /implib:"%BIN_DIR%\dsOpenAL.lib" %LIBS% /libpath:"%OPENAL_LIB%" %BIN_DIR%\doomsday.lib openal32.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** dsDirectSound.dll
:dsDirectSound
ECHO ***************************************************************************
ECHO *****   Compiling dsDirectSound.dll (DirectSound(3D) SoundFX driver)   ****
ECHO ***************************************************************************
md %OBJ_DIR%\dsDirectSound
cl /O2 /Ob1 /I "%INCS_ENGINE_API%\\" /I "%DX_INC%" /I "%EAX_INC%" %BUILDDEFS% %DLLDEFINES% /D "DSDIRECTSOUND_EXPORTS" /GF /FD /EHsc /Gy /Fo"%OBJ_DIR%\dsDirectSound" /Fd"%OBJ_DIR%\dsDirectSound" /W3 /Gd "%OBJ_DIR%\dsdirectsound\dsdirectsound_res.obj" @dsdirectsound_cl.rsp  /link /out:"%BIN_DIR%\dsDirectSound.dll" %LFLAGS% /dll /def:"%DENG_PLUGINS_DIR%\directsound\api\dsdirectsound.def" /implib:"%BIN_DIR%\dsDirectSound.lib" %LIBS% /libpath:"%DX_LIB%\\" /libpath:"%EAX_LIB%" eax.lib eaxguid.lib dsound.lib dxguid.lib %BIN_DIR%\doomsday.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** dsWinMM.dll
:dsWinMM
ECHO ***************************************************************************
ECHO *******   Compiling dsWinMM.dll (Windows Multimedia Mixing driver)   ******
ECHO ***************************************************************************
md %OBJ_DIR%\dsWinMM
cl /O2 /Ob1 /I "%INCS_ENGINE_API%\\" /I "%DENG_PLUGINS_DIR%\winmm\include" %BUILDDEFS% %DLLDEFINES% /D "DSWINMM_EXPORTS" /GF /FD /EHsc /Gy /Fo"%OBJ_DIR%\dswinmm\\" /Fd"%OBJ_DIR%\dswinmm\\" /W3 /Gd "%OBJ_DIR%\dswinmm\dswinmm_res.obj" @dswinmm_cl.rsp  /link /out:"%BIN_DIR%\dsWinMM.dll" %LFLAGS% /dll /def:"%DENG_PLUGINS_DIR%\winmm\api\dswinmm.def" /implib:"%BIN_DIR%\dsWinMM.lib" %LIBS% winmm.lib %BIN_DIR%\doomsday.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** dpWadMapConverter.dll
:dpWadMapConverter
ECHO ***************************************************************************
ECHO *****   Compiling dpWadMapConverter.dll (WAD Map converter plugin)   ******
ECHO ***************************************************************************
md %OBJ_DIR%\dpWadMapConverter
cl /O2 /Ob1 /I "%INCS_ENGINE_API%\\" /I "%DENG_PLUGINS_DIR%\wadmapconverter\include" %BUILDDEFS% %DLLDEFINES% /D "DPWADMAPCONVERTER_EXPORTS" /GF /FD /EHsc /Gy /Fo"%OBJ_DIR%\dpwadmapconverter\\" /Fd"%OBJ_DIR%\dpWadMapConverter\\" /W3 /Gd "%OBJ_DIR%\dpwadmapconverter\dpwadmapconverter_res.obj" @dpwadmapconverter_cl.rsp  /link /out:"%BIN_DIR%\dpWadMapConverter.dll" %LFLAGS% /dll /def:"%DENG_PLUGINS_DIR%\wadmapconverter\api\dpwadmapconverter.def" /implib:"%BIN_DIR%\dpwadmapconverter.lib" %LIBS% %BIN_DIR%\doomsday.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** jDoom.dll
:jDoom
ECHO ***************************************************************************
ECHO **************   Compiling jDoom.dll (jDoom Game Library)   ***************
ECHO ***************************************************************************
md %OBJ_DIR%\jDoom
cl /O2 /Ob1 /I "%INCS_PLUGIN_COMMON%\\" /I "%INCS_ENGINE_API%\\" /I "%INCS_LZSS_PORTABLE%\\" /I "%DENG_PLUGINS_DIR%\jdoom\include" /D "__JDOOM__" %BUILDDEFS% %DLLDEFINES% /D "JDOOM_EXPORTS" /GF /FD /EHsc /Gy /Fo"%OBJ_DIR%\jDoom\\" /Fd"%OBJ_DIR%\jDoom\\" /W3 /Gd "%OBJ_DIR%\jdoom\jdoom_res.obj" @jdoom_cl.rsp  /link  /out:"%BIN_DIR%\jDoom.dll" %LFLAGS% /libpath:".\Lib" /dll /def:"%DENG_PLUGINS_DIR%\jdoom\api\jdoom.def" /implib:"%BIN_DIR%\jDoom.lib" "%BIN_DIR%\doomsday.lib" "%DENG_EXTERNAL_DIR%\lzss\win32\lzss.lib"
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** jHeretic.dll
:jHeretic
ECHO ***************************************************************************
ECHO ***********   Compiling jHeretic.dll (jHeretic Game Library)   ************
ECHO ***************************************************************************
md %OBJ_DIR%\jHeretic
cl /O2 /Ob1 /I "%INCS_PLUGIN_COMMON%\\" /I "%INCS_ENGINE_API%\\" /I "%INCS_LZSS_PORTABLE%\\" /I "%DENG_PLUGINS_DIR%\jheretic\include" /D "__JHERETIC__" %BUILDDEFS% %DLLDEFINES% /D "JHERETIC_EXPORTS" /GF /FD /EHsc /Gy /Fo"%OBJ_DIR%\jHeretic\\" /Fd"%OBJ_DIR%\jHeretic\\" /W3 /Gd "%OBJ_DIR%\jheretic\jheretic_res.obj" @jheretic_cl.rsp  /link /out:"%BIN_DIR%\jHeretic.dll" %LFLAGS% /libpath:".\Lib" /dll /def:"%DENG_PLUGINS_DIR%\jheretic\api\jheretic.def" /implib:"%BIN_DIR%\jHeretic.lib" %BIN_DIR%\doomsday.lib %DENG_EXTERNAL_DIR%\lzss\win32\lzss.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** jHexen.dll
:jHexen
ECHO ***************************************************************************
ECHO *************   Compiling jHexen.dll (jHexen Game Library)   **************
ECHO ***************************************************************************
md %OBJ_DIR%\jHexen
cl /O2 /Ob1 /I "%INCS_PLUGIN_COMMON%\\" /I "%INCS_ENGINE_API%\\" /I "%INCS_LZSS_PORTABLE%\\" /I "%DENG_PLUGINS_DIR%\jhexen\include" /D "__JHEXEN__" %BUILDDEFS% %DLLDEFINES% /D "JHEXEN_EXPORTS" /GF /FD /EHsc /Gy /Fo"%OBJ_DIR%\jHexen\\" /Fd"%OBJ_DIR%\jHexen\\" /W3 /Gd "%OBJ_DIR%\jhexen\jhexen_res.obj" @jhexen_cl.rsp  /link /out:"%BIN_DIR%\jHexen.dll" %LFLAGS% /libpath:".\Lib" /dll /def:"%DENG_PLUGINS_DIR%\jhexen\api\jhexen.def" /implib:"%BIN_DIR%\jHexen.lib" %BIN_DIR%\doomsday.lib %DENG_EXTERNAL_DIR%\lzss\win32\lzss.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** jDoom64.dll
:jDoom64
ECHO ***************************************************************************
ECHO ************   Compiling jDoom64.dll (jDoom64 Game Library)   *************
ECHO ***************************************************************************
md %OBJ_DIR%\jDoom64
cl /O2 /Ob1 /I "%INCS_PLUGIN_COMMON%\\" /I "%INCS_ENGINE_API%\\" /I "%INCS_LZSS_PORTABLE%\\" /I "%DENG_PLUGINS_DIR%\jdoom64\include" /D "__JDOOM64__" %BUILDDEFS% %DLLDEFINES% /D "JDOOM64_EXPORTS" /GF /FD /EHsc /Gy /Fo"%OBJ_DIR%\jDoom64\\" /Fd"%OBJ_DIR%\jDoom64\\" /W3 /Gd "%OBJ_DIR%\jdoom64\jdoom64_res.obj" @jdoom64_cl.rsp  /link  /out:"%BIN_DIR%\jDoom64.dll" %LFLAGS% /libpath:".\Lib" /dll /def:"%DENG_PLUGINS_DIR%\jdoom64\api\jdoom64.def" /implib:"%BIN_DIR%\jDoom64.lib" %BIN_DIR%\doomsday.lib %DENG_EXTERNAL_DIR%\lzss\win32\lzss.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** dpExample.dll
:dpExample
ECHO ***************************************************************************
ECHO **********   Compiling dpExample.dll (Doomsday Example Plugin)   **********
ECHO ***************************************************************************
md %OBJ_DIR%\dpExample
cl /O2 /Ob1 /I "%INCS_ENGINE_API%\\" /I "%DENG_PLUGINS_DIR%\exampleplugin\include" %BUILDDEFS% %DLLDEFINES% /D "__EXAMPLE_PLUGIN__" /D "DPEXAMPLE_EXPORTS" /GF /FD /EHsc /Gy /Fo"%OBJ_DIR%\dpExample\\" /Fd"%OBJ_DIR%\dpExample\\" /W3 /Gd  @dpexample_cl.rsp  /link /out:"%BIN_DIR%\dpExample.dll" %LFLAGS% /dll /def:"%DENG_PLUGINS_DIR%\exampleplugin\api\dpExample.def" /implib:"%BIN_DIR%\dpExample.lib" %LIBS% %BIN_DIR%\Doomsday.lib %LIBS%
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: -- Move to the next argument.
:Done
SHIFT
GOTO Looper


:Failure
SET SOMETHING_FAILED=1
ECHO.
ECHO.
ECHO ***************************************************************************
ECHO ****************************   BUILD FAILED!   ****************************
ECHO ***************************************************************************
ECHO.
ECHO.
GOTO Done


:Failure2
ECHO There were build errors.
EXIT /b 1


:TheEnd
IF %SOMETHING_FAILED% == 1 GOTO Failure2
ECHO All Done!


:TheRealEnd
