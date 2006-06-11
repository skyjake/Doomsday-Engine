@ECHO OFF

REM --- This batch file should be run in the Doomsday base directory,
REM --- i.e. the folder where you installed the game (the default is
REM --- C:\Doomsday).

REM --- NOTES:
REM --- -sbd (-stdbasedir) is equal to "-basedir ..\.."
REM --- "-gl drOpenGL.dll" is the default, so it could be omitted.

Bin\Doomsday -game jDoom.dll -gl drOpenGL.dll -sbd -userdir Run\jDoom %1 %2 %3 %4 %5 %6 %7 %8 %9

