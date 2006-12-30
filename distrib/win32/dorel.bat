REM -- Does a complete Win32 Binary Release distribution.

REM -- Package a Snowberry binary.
cd ..\..\snowberry
call build.bat
cd ..\distrib\win32

REM -- Recompile resource packages.
cd ..\..\doomsday\build\scripts
packres.py ../win32
cd ..\..\..\distrib\win32

REM -- Recompile.
cd ..\..\doomsday\build\win32
call vcbuild.bat all
cd ..\..\..\distrib\win32

REM -- Run the Inno Setup Compiler.
"C:\Program Files\Inno Setup 5\Compil32.exe" /cc setup.iss
