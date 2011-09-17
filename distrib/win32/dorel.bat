@echo off
REM -- Does a complete Win32 Binary Release distribution.

REM -- Set up the build environment.
call ..\..\doomsday\build\win32\envconfig.bat

REM -- Build number.
SET DOOMSDAY_BUILD=%1
echo Doomsday build number is %DOOMSDAY_BUILD%.

REM -- Package a Snowberry binary.
cd ..\..\snowberry
call build.bat
cd ..\distrib\win32

REM -- Recompile.
SET BUILDFAILURE=0
rd/s/q work
md work
cd work
qmake ..\..\..\doomsday\doomsday.pro CONFIG+=release DENG_BUILD=%DOOMSDAY_BUILD%
IF %ERRORLEVEL% == 1 SET BUILDFAILURE=1
%JOM%
IF %ERRORLEVEL% == 1 SET BUILDFAILURE=1
%JOM% install
IF %ERRORLEVEL% == 1 SET BUILDFAILURE=1
cd ..
rd/s/q work

IF %BUILDFAILURE% == 1 GOTO Failure

REM -- Run the Inno Setup Compiler.
"C:\Program Files\Inno Setup 5\Compil32.exe" /cc setup.iss

goto TheEnd

:Failure
echo Failure during build!
exit /b 1

:TheEnd
