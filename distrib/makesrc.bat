@ECHO OFF
REM --- Build the source package (with everything!).
REM --- Includes MSVC files, sources (.C, .H), documentation and data files.

ECHO ========================== MAKING SOURCE PACKAGE ===========================

REM -- Prepare output directory.
md Out
SET THISDIR=distrib
SET FILE=..\%THISDIR%\Out\deng-src-1.zip

cd ..\doomsday

SET _DataPath=Data
SET _UtilPath=Utils

REM --- Pack it with WZZIP.

del %FILE%

wzzip -a -ex -P -r -xa3dapi.dll %FILE% LICENSE *.sln *.vcproj *.dsp *.dsw Src\*.c Src\*.cpp Src\*.h Src\*.def Src\*.rc Src\*.rc2 Include\*.h Doc\SrcNotes.txt Doc\Readme.txt Doc\jDoom\Doomlic.txt Doc\Ravenlic.txt Doc\CmdLine.txt Doc\Beginner.txt Doc\Network.txt Doc\Example.bat Doc\DSS.txt Doc\DHistory.txt Doc\ChangeLog.txt Doc\DEDDoc.txt Doc\jDoom\History.txt Doc\jHeretic\History.txt Doc\jHexen\History.txt Src\*.ico Src\*.bmp Data\KeyMaps\*.dkm %_DataPath%\*.ico Runtime\jDoom\*.rsp Runtime\jHeretic\*.rsp Runtime\jHexen\*.rsp Runtime\jDoom\Startup.cfg Runtime\jHeretic\Startup.cfg Runtime\jHexen\Startup.cfg Res\*.ico

cd ..\%THISDIR%
