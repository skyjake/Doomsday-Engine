@ECHO OFF
REM --- Build the binary snapshot.
REM --- %1 optional; "nodll" will not add rendering and sound DLLs

ECHO ========================= MAKING BINARY SNAPSHOT ==========================

SET RAR=-m5

REM --- Pack it with RAR.

SET OUT=Out\
md %OUT%

SET SRC=..\doomsday\
SET FILE=%OUT%ddsnapshot
del %FILE%.rar

REM --- The files

rar -ep -r a %FILE% %SRC%Bin\Release\Doomsday.exe %SRC%Bin\Release\jDoom.dll %SRC%Bin\Release\jHeretic.dll %SRC%Bin\Release\jHexen.dll

if "%1"=="nodll" goto skipdll
rar -ep -r a %FILE% %SRC%Bin\Release\jtNet2.dll %SRC%Bin\Release\drOpenGL.dll %SRC%Bin\Release\drD3D.dll %SRC%Bin\Release\dsA3D.dll %SRC%Bin\Release\dsCompat.dll
:skipdll

echo Uploading to The Mirror...
ftpscrpt -f z:\scripts\mirror-snapshot.scp

echo Uploading to Fourwinds...
ftpscrpt -f z:\scripts\snapshot.scp

echo Done.

