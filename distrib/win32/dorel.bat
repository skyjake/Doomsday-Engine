@echo off
REM -- Does a complete Win32 Binary Release distribution.

REM -- Visual C++ environment.
call "c:\Program Files\Microsoft Visual Studio 10.0\vc\vcvarsall.bat"

REM -- Build number.
SET DOOMSDAY_BUILD=%1
echo Doomsday build number is %DOOMSDAY_BUILD%.

REM -- Package a Snowberry binary.
cd ..\..\snowberry
call build.bat
cd ..\distrib\win32

REM -- Recompile resource packages.
cd ..\..\doomsday\build\scripts
packres.py ../win32
cd ..\..\..\distrib\win32

REM -- Recompile.
SET BUILDFAILURE=0
cd ..\..\doomsday\build\win32
call vcbuild.bat all
IF %ERRORLEVEL% == 1 SET BUILDFAILURE=1
cd ..\..\..\distrib\win32

IF %BUILDFAILURE% == 1 GOTO Failure

REM -- Run the Inno Setup Compiler.
"C:\Program Files\Inno Setup 5\Compil32.exe" /cc setup.iss

goto TheEnd

:Failure
exit /b 1

:TheEnd
