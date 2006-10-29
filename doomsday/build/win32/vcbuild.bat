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

:: -- Compiler and linker options.
SET DEFINES=/D "ZLIB_DLL" /D "WIN32_GAMMA" /D "NORANGECHECKING" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS"
SET DLLDEFINES=/D "_USRDLL" /D "_WINDLL" %DEFINES%
SET INCS=/I "%EAX_INC%" /I "%SDL_INC%" /I "%SDLNET_INC%" /I "%DX_INC%" /I "%PLATFORM_INC%" /I "./../../engine/api"
SET INCS_ENGINE_API=/I "./../../engine/api"
SET INCS_ENGINE_PORTABLE=/I "./../../engine/portable/include"
SET INCS_ENGINE_WIN32=/I "./../../engine/win32/include"
SET INCS_LZSS_PORTABLE=/I "./../../external/lzss/portable/include"
SET INCS_LIBPNG_PORTABLE=/I "./../../external/libpng/portable/include"
SET INCS_ZLIB=/I "./../../external/zlib/include"
SET INCS_PLUGIN_COMMON=/I "./../../plugins/common/include"
SET LIBS=/LIBPATH:"./Lib" /LIBPATH:"%DX_LIB%" /LIBPATH:"%EAX_LIB%" /LIBPATH:"%SDL_LIB%" /LIBPATH:"%SDLNET_LIB%" /LIBPATH:"%PLATFORM_LIB%" /LIBPATH:"./%BIN_DIR%"
SET FLAGS=/Ob1 /Oi /Ot /Oy /GF /FD /EHsc /MT /GS /Gy /Fo"./Obj/Release/" /Fd"./Obj/Release/" /W3 /Gz /Gs
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
CALL vcbuild cleanup res dmt doomsday dpdehread dpmapload dropengl drd3d dsa3d dscompat jdoom jheretic jhexen wolftc doom64tc
GOTO Done


:: *** Clean the output directories.
:Cleanup
rd/s/q %BIN_DIR%
rd/s/q %OBJ_DIR%
md %BIN_DIR%
md %OBJ_DIR%
GOTO Done

:: *** Resources (dialogs for Doomsday and drD3D)
:: Requires rc.exe and cvtres.exe from the Platform SDK.
:Res
ECHO Generating resources for Doomsday (doomsday_res.obj)...
rc /i "%PLATFORM_INC%" ./../../engine/win32/res/Doomsday.rc
cvtres /OUT:%OBJ_DIR%doomsday_res.obj /MACHINE:X86 ./../../engine/win32/res/Doomsday.res
ECHO.
ECHO Generating resources for drD3D (drD3D_res.obj)...
rc /i "%PLATFORM_INC%" ./../../plugins/d3d/res/drD3D.rc
IF NOT EXIST %OBJ_DIR%/drD3D md %OBJ_DIR%/drD3D
cvtres /OUT:%OBJ_DIR%/drD3D/drD3D_res.obj /MACHINE:X86 ./../../plugins/d3d/res/drD3D.res
GOTO Done


:: *** Execute a Python Script
:: Requires Python interpreter.
:Script
ECHO Run Script: %2
SET CURRENTSCRIPT=%2
%PYTHON_DIR%\python "%CURRENTSCRIPT%"
SHIFT
GOTO Done


:: *** Mapdata type headers
:CheckDMT
:: First check for their existence
IF NOT EXIST ../../engine/api/dd_maptypes.h GOTO DMT
IF NOT EXIST ../../engine/portable/include/p_maptypes.h GOTO DMT
:: TODO compare creation date&time with that of mapdata.hs
:: If mapdata.hs is newer - recreate the headers.
ECHO  DMT headers are upto date.
GOTO TheRealEnd

:DMT
CALL vcbuild script "../../engine/scripts/makedmt.py <../../engine/portable/include/mapdata.hs"
MOVE /Y dd_maptypes.h ../../engine/api\
MOVE /Y p_maptypes.h ../../engine/portable/include\
GOTO DONE


:: *** Doomsday.exe
:Doomsday
CALL vcbuild checkdmt
ECHO Compiling Doomsday.exe (Engine)...
cl %FLAGS% %INCS% %INCS_ENGINE_WIN32% %INCS_ENGINE_PORTABLE% %INCS_LZSS_PORTABLE% %INCS_LIBPNG_PORTABLE% %INCS_ZLIB% %INCS_PLUGIN_COMMON% %DEFINES% %DEBUGDEFINES% %PROFILEDEFINES% /D "__DOOMSDAY__"  ./%OBJ_DIR%/doomsday_res.obj  @doomsday_cl.rsp    /link /OUT:"./%BIN_DIR%/Doomsday.exe" /DEF:"./../../engine/api/doomsday.def" /IMPLIB:"./%BIN_DIR%/Doomsday.lib" %LFLAGS% %LIBS% sdl_net.lib sdl.lib wsock32.lib %EXTERNAL%/libpng/win32/libpng.lib %EXTERNAL%/zlib/win32/libz.lib %EXTERNAL%/lzss/win32/lzss.lib dinput.lib dsound.lib eaxguid.lib dxguid.lib winmm.lib %EXTERNAL%/libpng/win32/libpng.lib %EXTERNAL%/zlib/win32/libz.lib %EXTERNAL%/lzss/win32/LZSS.lib gdi32.lib ole32.lib user32.lib
GOTO Done


:: *** dpDehRead.dll
:dpDehRead
ECHO Compiling dpDehRead.dll (Dehacked Reader Plugin)...
md %OBJ_DIR%\dpDehRead
cl /O2 /Ob1 %INCS% %DLLDEFINES% /D "DPDEHREAD_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dpDehRead/" /Fd"./%OBJ_DIR%/dpDehRead/" /W3 /Gz   @dpdehread_cl.rsp   /link /OUT:"./%BIN_DIR%/dpDehRead.dll" %LFLAGS% /DLL /IMPLIB:"./%BIN_DIR%/dpDehRead.lib" %LIBS% ./%BIN_DIR%/Doomsday.lib
GOTO Done


:: *** dpMapLoad.dll
:dpMapLoad
ECHO Compiling dpMapLoad.dll ((gl)BSP Node Builder Plugin)...
md %OBJ_DIR%\dpMapLoad
cl /O2 /Ob1 %INCS_ZLIB% %INCS% /I "./../../external/glbsp/include" %DLLDEFINES% /D "GLBSP_PLUGIN" /D "DPMAPLOAD_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dpMapLoad/" /Fd"./%OBJ_DIR%/dpMapLoad/" /W3 /Gz  @dpmapload_cl.rsp  /link /OUT:"./%BIN_DIR%/dpMapLoad.dll" %LFLAGS% /DLL /IMPLIB:"./%BIN_DIR%/dpMapLoad.lib" %LIBS% ./%BIN_DIR%/Doomsday.lib 
GOTO Done


:: *** drD3D.dll
:drD3D
ECHO Compiling drD3D.dll (Direct3D 8.1 driver)...
md %OBJ_DIR%\drD3D
cl /O2 /Ob1 /I "./../../plugins/d3d/include" %INCS% %DLLDEFINES% /D "drD3D_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/drD3D/" /Fd"./%OBJ_DIR%/drD3D/" /W3 ./%OBJ_DIR%/drD3D/drD3D_res.obj  @drd3d_cl.rsp  /link /OUT:"./%BIN_DIR%/drD3D.dll" %LFLAGS% /DLL /DEF:"./../../plugins/d3d/api/drD3D.def" /IMPLIB:"./%BIN_DIR%/drD3D.lib" /LIBPATH:"%LIBCI_LIB%" %LIBS% ./%BIN_DIR%/doomsday.lib d3d8.lib d3dx8.lib user32.lib gdi32.lib ole32.lib uuid.lib advapi32.lib
GOTO Done


:: *** drOpenGL.dll
:drOpenGL
ECHO Compiling drOpenGL.dll (OpenGL driver)...
md %OBJ_DIR%\drOpenGL
cl /O2 /Ob1 /I "%GL_INC%" /I "./../../plugins/opengl/portable/include" %INCS% %DLLDEFINES% /D "drOpenGL_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/drOpenGL/" /Fd"./%OBJ_DIR%/drOpenGL/" /W3 /Gz  @dropengl_cl.rsp  /link /OUT:"./%BIN_DIR%/drOpenGL.dll" %LFLAGS% /DLL /DEF:"./../../plugins/opengl/api/drOpenGL.def" /IMPLIB:"./%BIN_DIR%/drOpenGL.lib" %LIBS% ./%BIN_DIR%/doomsday.lib opengl32.lib glu32.lib kernel32.lib user32.lib gdi32.lib
GOTO Done


:: *** dsA3D.dll
:dsA3D
ECHO Compiling dsA3D.dll (A3D SoundFX driver)...
md %OBJ_DIR%\dsA3D
cl /O2 /Ob1 %INCS% /I "%A3D_INC%" %DLLDEFINES% /D "DSA3D_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dsA3D" /Fd"./%OBJ_DIR%/dsA3D" /W3 /Gz  @dsa3d_cl.rsp  /link /OUT:"./%BIN_DIR%/dsA3D.dll" %LFLAGS% /DLL /DEF:"./../../plugins/a3d/api/dsA3D.def" /IMPLIB:"./%BIN_DIR%/dsA3D.lib" %LIBS% /LIBPATH:"%A3D_LIB%" ./%BIN_DIR%/doomsday.lib ia3dutil.lib ole32.lib 
GOTO Done


:: *** dsCompat.dll
:dsCompat
ECHO Compiling dsCompat.dll (DirectSound(3D) SoundFX driver)...
md %OBJ_DIR%\dsCompat
cl /O2 /Ob1 %INCS% %DLLDEFINES% /D "DSCOMPAT_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dsCompat" /Fd"./%OBJ_DIR%/dsCompat" /W3 /Gz  @dscompat_cl.rsp  /link /OUT:"./%BIN_DIR%/dsCompat.dll" %LFLAGS% /DLL /DEF:"./../../plugins/ds6/api/dsCompat.def" /IMPLIB:"./%BIN_DIR%/dsCompat.lib" %LIBS% eax.lib eaxguid.lib dsound.lib dxguid.lib ./%BIN_DIR%/doomsday.lib 
GOTO Done


:: *** jDoom.dll
:jDoom
ECHO Compiling jDoom.dll (jDoom Game Library)...
md %OBJ_DIR%\jDoom
cl /O2 /Ob1 %INCS_PLUGIN_COMMON% %INCS_ENGINE_API% %INCS_LZSS_PORTABLE% /I "./../../plugins/jdoom/include" /D "__JDOOM__" %DLLDEFINES% /D "JDOOM_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/jDoom/" /Fd"./%OBJ_DIR%/jDoom/" /W3 /Gz  @jdoom_cl.rsp  /link  /OUT:"./%BIN_DIR%/jDoom.dll" %LFLAGS% /LIBPATH:"./Lib" /DLL /DEF:"./../../plugins/jdoom/api/jdoom.def" /IMPLIB:"./%BIN_DIR%/jDoom.lib" ./%BIN_DIR%/doomsday.lib %EXTERNAL%/lzss/win32/lzss.lib
GOTO Done


:: *** jHeretic.dll
:jHeretic
ECHO Compiling jHeretic.dll (jHeretic Game Library)...
md %OBJ_DIR%\jHeretic
cl /O2 /Ob1 %INCS_PLUGIN_COMMON% %INCS_ENGINE_API% %INCS_LZSS_PORTABLE% /I "./../../plugins/jheretic/include" /D "__JHERETIC__" %DLLDEFINES% /D "JHERETIC_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/jHeretic/" /Fd"./%OBJ_DIR%/jHeretic/" /W3 /Gz  @jheretic_cl.rsp  /link /OUT:"./%BIN_DIR%/jHeretic.dll" %LFLAGS% /LIBPATH:"./Lib" /DLL /DEF:"./../../plugins/jheretic/api/jheretic.def" /IMPLIB:"./%BIN_DIR%/jHeretic.lib" ./%BIN_DIR%/doomsday.lib %EXTERNAL%/lzss/win32/lzss.lib 
GOTO Done


:: *** jHexen.dll
:jHexen
ECHO Compiling jHexen.dll (jHexen Game Library)...
md %OBJ_DIR%\jHexen
cl /O2 /Ob1 %INCS_PLUGIN_COMMON% %INCS_ENGINE_API% %INCS_LZSS_PORTABLE% /I "./../../plugins/jhexen/include" /D "__JHEXEN__" %DLLDEFINES% /D "JHEXEN_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/jHexen/" /Fd"./%OBJ_DIR%/jHexen/" /W3 /Gz  @jhexen_cl.rsp  /link /OUT:"./%BIN_DIR%/jHexen.dll" %LFLAGS% /LIBPATH:"./Lib" /DLL /DEF:"./../../plugins/jhexen/api/jhexen.def" /IMPLIB:"./%BIN_DIR%/jHexen.lib" ./%BIN_DIR%/doomsday.lib %EXTERNAL%/lzss/win32/lzss.lib
GOTO Done


:: *** WolfTC.dll
:WolfTC
ECHO Compiling WolfTC.dll (WolfTC Game Library)...
md %OBJ_DIR%\WolfTC
cl /O2 /Ob1 %INCS_PLUGIN_COMMON% %INCS_ENGINE_API% %INCS_LZSS_PORTABLE% /I "./../../plugins/wolftc/include" /D "__WOLFTC__" /D "__JDOOM__" %DLLDEFINES% /D "WOLFTC_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/WolfTC/" /Fd"./%OBJ_DIR%/WolfTC/" /W3 /Gz  @wolftc_cl.rsp  /link  /OUT:"./%BIN_DIR%/WolfTC.dll" %LFLAGS% /LIBPATH:"./Lib" /DLL /DEF:"./../../plugins/wolftc/api/wolftc.def" /IMPLIB:"./%BIN_DIR%/WolfTC.lib" ./%BIN_DIR%/doomsday.lib %EXTERNAL%/lzss/win32/lzss.lib
GOTO Done


:: *** Doom64TC.dll
:Doom64TC
ECHO Compiling Doom64TC.dll (Doom64TC Game Library)...
md %OBJ_DIR%\Doom64TC
cl /O2 /Ob1 %INCS_PLUGIN_COMMON% %INCS_ENGINE_API% %INCS_LZSS_PORTABLE% /I "./../../plugins/doom64tc/include" /D "__DOOM64TC__" /D "__JDOOM__" %DLLDEFINES% /D "DOOM64TC_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/Doom64TC/" /Fd"./%OBJ_DIR%/Doom64TC/" /W3 /Gz  @doom64tc_cl.rsp  /link  /OUT:"./%BIN_DIR%/Doom64TC.dll" %LFLAGS% /LIBPATH:"./Lib" /DLL /DEF:"./../../plugins/doom64tc/api/doom64tc.def" /IMPLIB:"./%BIN_DIR%/Doom64TC.lib" ./%BIN_DIR%/doomsday.lib %EXTERNAL%/lzss/win32/lzss.lib
GOTO Done


:: -- Move to the next argument.
:Done
SHIFT
GOTO Looper


:TheEnd
ECHO All Done!


:TheRealEnd
