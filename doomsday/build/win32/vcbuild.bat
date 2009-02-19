@ECHO OFF
IF "%1" == "" GOTO Help
IF "%1" == "help" GOTO Help
GOTO SetPaths
:Help
cls
ECHO $Id$
ECHO.
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

:: -- Compiler and linker options.
SET DEFINES=/D "ZLIB_DLL" /D "WIN32_GAMMA" /D "NORANGECHECKING" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_CRT_SECURE_NO_DEPRECATE"
SET DLLDEFINES=/D "_USRDLL" /D "_WINDLL" %DEFINES%
SET INCS=/I "%EAX_INC%" /I "%SDL_INC%" /I "%SDLMIXER_INC%" /I "%SDLNET_INC%" /I "%DX_INC%" /I "%PLATFORM_INC%" /I "./../../engine/api"
SET INCS_ENGINE_API=/I "./../../engine/api"
SET INCS_ENGINE_PORTABLE=/I "./../../engine/portable/include"
SET INCS_ENGINE_WIN32=/I "./../../engine/win32/include"
SET INCS_LZSS_PORTABLE=/I "./../../external/lzss/portable/include"
SET INCS_LIBPNG_PORTABLE=/I "./../../external/libpng/portable/include"
SET INCS_ZLIB=/I "./../../external/zlib/portable/include"
SET INCS_LIBCURL=/I "./../../external/libcurl/portable/include"
SET INCS_PLUGIN_COMMON=/I "./../../plugins/common/include"
SET LIBS=/LIBPATH:"./Lib" /LIBPATH:"%DX_LIB%" /LIBPATH:"%EAX_LIB%" /LIBPATH:"%SDL_LIB%" /LIBPATH:"%SDLNET_LIB%" /LIBPATH:"%SDLMIXER_LIB%" /LIBPATH:"%PLATFORM_LIB%" /LIBPATH:"./%BIN_DIR%"
SET FLAGS=/Ob1 /Oi /Ot /Oy /GF /FD /EHsc /MT /GS /Gy /Fo"./Obj/Release/" /Fd"./Obj/Release/" /W3 /Gd /Gs
SET LFLAGS=/INCREMENTAL:NO /SUBSYSTEM:WINDOWS /MACHINE:I386
SET EXTERNAL=./../../external


:: -- Make sure the output directories exist.
IF NOT EXIST %BIN_DIR% md %BIN_DIR%
IF NOT EXIST %OBJ_DIR% md %OBJ_DIR%


:: -- Iterate through all command line arguments.
:Looper
IF "%1" == "" GOTO TheEnd
GOTO %1


:: *** Build options
:_D
SET DEBUGDEFINES=/D "_DEBUG"
GOTO Done
:_P
SET PROFILEDEFINES=/D "DD_PROFILE"
GOTO Done

:: *** Cleanup and build all targets.
:All
CALL vcbuild.bat cleanup copydll res dmt doomsday dpdehread dpwadmapconverter dswinmm dsdirectsound jdoom jheretic jhexen jdoom64
GOTO Done


:: *** Clean the output directories.
:Cleanup
rd/s/q %BIN_DIR%
rd/s/q %OBJ_DIR%
md %BIN_DIR%
md %OBJ_DIR%
GOTO Done


:: *** Resources (dialogs for Doomsday)
:: Requires rc.exe and cvtres.exe from the Platform SDK.
:Res
ECHO Generating resources for Doomsday (doomsday_res.obj)...
rc /i "%PLATFORM_INC%" /i "%PLATFORM_INC%\mfc" ./../../engine/win32/res/Doomsday.rc
cvtres /OUT:%OBJ_DIR%\doomsday_res.obj /MACHINE:X86 ./../../engine/win32/res/Doomsday.res
GOTO Done


:: *** Execute a Python Script
:: Requires Python interpreter.
:Script
ECHO Run Script: %2
SET CURRENTSCRIPT=%2
"%PYTHON_DIR%"\python "%CURRENTSCRIPT%"
SHIFT
GOTO Done


:: *** Copy DLLs to the build directory, for packaging.
:copydll
copy %SDL_LIB%\SDL.dll .
copy %SDLNET_LIB%\SDL_net.dll .
copy %SDLMIXER_LIB%\SDL_mixer.dll .
copy %SDLMIXER_LIB%\ogg.dll .
copy %SDLMIXER_LIB%\smpeg.dll .
copy %SDLMIXER_LIB%\vorbis.dll .
copy %SDLMIXER_LIB%\vorbisfile.dll .
copy %EAX_DLL%\eax.dll .
GOTO Done

:: *** Mapdata type headers
:CheckDMT
:: First check for their existence
IF NOT EXIST ../../engine/api/dd_maptypes.h GOTO DMT
IF NOT EXIST ../../engine/portable/include/p_maptypes.h GOTO DMT
:: TODO compare creation date&time with that of mapdata.hs
:: If mapdata.hs is newer - recreate the headers.
ECHO  DMT headers are up to date.
GOTO TheRealEnd

:DMT
CALL vcbuild.bat script "../../engine/scripts/makedmt.py <../../engine/portable/include/mapdata.hs"
MOVE /Y dd_maptypes.h ../../engine/api\
MOVE /Y p_maptypes.h ../../engine/portable/include\
GOTO DONE


:: *** Doomsday.exe
:Doomsday
CALL vcbuild.bat checkdmt
ECHO Compiling Doomsday.exe (Engine)...
cl %FLAGS% %INCS% %INCS_ENGINE_WIN32% /I "%GL_INC%" %INCS_ENGINE_PORTABLE% %INCS_LZSS_PORTABLE% %INCS_LIBPNG_PORTABLE% %INCS_LIBCURL% %INCS_ZLIB% %INCS_PLUGIN_COMMON% %DEFINES% %DEBUGDEFINES% %PROFILEDEFINES% /D "__DOOMSDAY__"  ./%OBJ_DIR%/doomsday_res.obj  @doomsday_cl.rsp    /link /OUT:"./%BIN_DIR%/Doomsday.exe" /DEF:"./../../engine/api/doomsday.def" /IMPLIB:"./%BIN_DIR%/Doomsday.lib" %LFLAGS% %LIBS% %EXTERNAL%/libpng/win32/libpng13.lib %EXTERNAL%/zlib/win32/zlib1.lib %EXTERNAL%/lzss/win32/lzss.lib %EXTERNAL%/libcurl/win32/curllib.lib sdl_net.lib sdl.lib SDL_mixer.lib wsock32.lib dinput8.lib dsound.lib eaxguid.lib dxguid.lib winmm.lib opengl32.lib glu32.lib kernel32.lib gdi32.lib ole32.lib user32.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** dpDehRead.dll
:dpDehRead
ECHO Compiling dpDehRead.dll (Dehacked Reader Plugin)...
md %OBJ_DIR%\dpDehRead
cl /O2 /Ob1 %INCS% %DLLDEFINES% /D "DPDEHREAD_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dpDehRead/" /Fd"./%OBJ_DIR%/dpDehRead/" /W3 /Gd   @dpdehread_cl.rsp   /link /OUT:"./%BIN_DIR%/dpDehRead.dll" %LFLAGS% /DLL /IMPLIB:"./%BIN_DIR%/dpDehRead.lib" %LIBS% ./%BIN_DIR%/Doomsday.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** dsOpenAL.dll
:dsOpenAL
ECHO Compiling dsOpenAL.dll (OpenAL SoundFX driver)...
md %OBJ_DIR%\dsOpenAL
cl /O2 /Ob1 %INCS% %DLLDEFINES% /D "DSOPENAL_EXPORTS" /I "%OPENAL_INC%" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dsOpenAL" /Fd"./%OBJ_DIR%/dsOpenAL" /W3 /Gd  @dsopenal_cl.rsp  /link /OUT:"./%BIN_DIR%/dsOpenAL.dll" %LFLAGS% /DLL /DEF:"./../../plugins/openal/api/dsOpenAL.def" /IMPLIB:"./%BIN_DIR%/dsOpenAL.lib" %LIBS% /LIBPATH:"%OPENAL_LIB%" ./%BIN_DIR%/doomsday.lib openal32.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** dsDirectSound.dll
:dsDirectSound
ECHO Compiling dsDirectSound.dll (DirectSound(3D) SoundFX driver)...
md %OBJ_DIR%\dsDirectSound
cl /O2 /Ob1 %INCS% %DLLDEFINES% /D "DSDIRECTSOUND_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dsDirectSound" /Fd"./%OBJ_DIR%/dsDirectSound" /W3 /Gd  @dsdirectsound_cl.rsp  /link /OUT:"./%BIN_DIR%/dsDirectSound.dll" %LFLAGS% /DLL /DEF:"./../../plugins/directsound/api/dsdirectsound.def" /IMPLIB:"./%BIN_DIR%/dsDirectSound.lib" %LIBS% eax.lib eaxguid.lib dsound.lib dxguid.lib ./%BIN_DIR%/doomsday.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** dsWinMM.dll
:dsWinMM
ECHO Compiling dsWinMM.dll (Windows Multimedia Mixing driver)...
md %OBJ_DIR%\dsWinMM
cl /O2 /Ob1 %INCS% /I "./../../plugins/winmm/include" %DLLDEFINES% /D "DSWINMM_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dswinmm/" /Fd"./%OBJ_DIR%/dswinmm/" /W3 /Gd  @dswinmm_cl.rsp  /link /OUT:"./%BIN_DIR%/dswinmm.dll" %LFLAGS% /DLL /DEF:"./../../plugins/winmm/api/dswinmm.def" /IMPLIB:"./%BIN_DIR%/dswinmm.lib" %LIBS% winmm.lib ./%BIN_DIR%/doomsday.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** dpWadMapConverter.dll
:dpWadMapConverter
ECHO Compiling dpWadMapConverter.dll (WAD Map converter plugin)...
md %OBJ_DIR%\dpWadMapConverter
cl /O2 /Ob1 %INCS% /I "./../../plugins/wadmapconverter/include" %DLLDEFINES% /D "DPWADMAPCONVERTER_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dpwadmapconverter/" /Fd"./%OBJ_DIR%/dpWadMapConverter/" /W3 /Gd  @dpwadmapconverter_cl.rsp  /link /OUT:"./%BIN_DIR%/dpWadMapConverter.dll" %LFLAGS% /DLL /IMPLIB:"./%BIN_DIR%/dpwadmapconverter.lib" %LIBS% ./%BIN_DIR%/doomsday.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** jDoom.dll
:jDoom
ECHO Compiling jDoom.dll (jDoom Game Library)...
md %OBJ_DIR%\jDoom
cl /O2 /Ob1 %INCS_PLUGIN_COMMON% %INCS_ENGINE_API% %INCS_LZSS_PORTABLE% /I "./../../plugins/jdoom/include" /D "__JDOOM__" %DLLDEFINES% /D "JDOOM_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/jDoom/" /Fd"./%OBJ_DIR%/jDoom/" /W3 /Gd  @jdoom_cl.rsp  /link  /OUT:"./%BIN_DIR%/jDoom.dll" %LFLAGS% /LIBPATH:"./Lib" /DLL /DEF:"./../../plugins/jdoom/api/jdoom.def" /IMPLIB:"./%BIN_DIR%/jDoom.lib" ./%BIN_DIR%/doomsday.lib %EXTERNAL%/lzss/win32/lzss.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** jHeretic.dll
:jHeretic
ECHO Compiling jHeretic.dll (jHeretic Game Library)...
md %OBJ_DIR%\jHeretic
cl /O2 /Ob1 %INCS_PLUGIN_COMMON% %INCS_ENGINE_API% %INCS_LZSS_PORTABLE% /I "./../../plugins/jheretic/include" /D "__JHERETIC__" %DLLDEFINES% /D "JHERETIC_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/jHeretic/" /Fd"./%OBJ_DIR%/jHeretic/" /W3 /Gd  @jheretic_cl.rsp  /link /OUT:"./%BIN_DIR%/jHeretic.dll" %LFLAGS% /LIBPATH:"./Lib" /DLL /DEF:"./../../plugins/jheretic/api/jheretic.def" /IMPLIB:"./%BIN_DIR%/jHeretic.lib" ./%BIN_DIR%/doomsday.lib %EXTERNAL%/lzss/win32/lzss.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** jHexen.dll
:jHexen
ECHO Compiling jHexen.dll (jHexen Game Library)...
md %OBJ_DIR%\jHexen
cl /O2 /Ob1 %INCS_PLUGIN_COMMON% %INCS_ENGINE_API% %INCS_LZSS_PORTABLE% /I "./../../plugins/jhexen/include" /D "__JHEXEN__" %DLLDEFINES% /D "JHEXEN_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/jHexen/" /Fd"./%OBJ_DIR%/jHexen/" /W3 /Gd  @jhexen_cl.rsp  /link /OUT:"./%BIN_DIR%/jHexen.dll" %LFLAGS% /LIBPATH:"./Lib" /DLL /DEF:"./../../plugins/jhexen/api/jhexen.def" /IMPLIB:"./%BIN_DIR%/jHexen.lib" ./%BIN_DIR%/doomsday.lib %EXTERNAL%/lzss/win32/lzss.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** jDoom64.dll
:jDoom64
ECHO Compiling jDoom64.dll (jDoom64 Game Library)...
md %OBJ_DIR%\jDoom64
cl /O2 /Ob1 %INCS_PLUGIN_COMMON% %INCS_ENGINE_API% %INCS_LZSS_PORTABLE% /I "./../../plugins/jdoom64/include" /D "__JDOOM64__" %DLLDEFINES% /D "JDOOM64_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/jDoom64/" /Fd"./%OBJ_DIR%/jDoom64/" /W3 /Gd  @jdoom64_cl.rsp  /link  /OUT:"./%BIN_DIR%/jDoom64.dll" %LFLAGS% /LIBPATH:"./Lib" /DLL /DEF:"./../../plugins/jdoom64/api/jdoom64.def" /IMPLIB:"./%BIN_DIR%/jDoom64.lib" ./%BIN_DIR%/doomsday.lib %EXTERNAL%/lzss/win32/lzss.lib
IF %ERRORLEVEL% == 0 GOTO Done
GOTO Failure


:: *** dpExample.dll
:dpExample
ECHO Compiling dpExample.dll (Doomsday Example Plugin)...
md %OBJ_DIR%\dpExample
cl /O2 /Ob1 %DLLDEFINES% /D "EXAMPLE_PLUGIN" /D "DPEXAMPLE_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dpExample/" /Fd"./%OBJ_DIR%/dpExample/" /W3 /Gd  @dpexample_cl.rsp  /link /OUT:"./%BIN_DIR%/dpExample.dll" %LFLAGS% /DLL /IMPLIB:"./%BIN_DIR%/dpExample.lib" %LIBS% ./%BIN_DIR%/Doomsday.lib
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
GOTO TheRealEnd


:TheEnd
IF %SOMETHING_FAILED% == 1 GOTO Failure2
ECHO All Done!


:TheRealEnd
