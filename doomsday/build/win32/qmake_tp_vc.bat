@echo off
REM -- Generates a Visual Studio solution and vcprojs for the project.
REM -- Note: Expected to be called by qmake_msvc.py.

REM -- Set up the build environment.
call ..\doomsday\build\win32\envconfig.bat

qmake -tp vc -r ..\doomsday\doomsday.pro
