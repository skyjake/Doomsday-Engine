@ECHO OFF
REM --- Build the source package (with everything!).
REM --- Includes MSVC files, sources (.C, .H), documentation and data files.

ECHO ========================== MAKING SOURCE PACKAGE ===========================

REM -- Prepare output directory.
md Out
SET THISDIR=distrib
SET FILE=..\%THISDIR%\Out\ddsrc_r%1

cd ..\doomsday

SET _LibPath=Lib
SET _DLLPath=DLLs
SET _DataPath=Data
SET _UtilPath=Utils

SET RAR=-s -m5 -sfx

REM --- Pack it with RAR.

del %FILE%.exe

REM --- MSVC and support files

rar -r a %FILE% LICENSE *.dsp *.dsw %_LibPath%\*.lib %_DLLPath%\*.dll -xa3dapi.dll

REM --- Source files

rar -r a %FILE% Src\*.c Src\*.cpp Src\*.h Src\*.def Src\*.rc Src\*.rc2 Include\*.h

REM --- Documentation files

rar -r a %FILE% Doc\SrcNotes.txt Doc\Readme.txt Doc\jDoom\Doomlic.txt Doc\Ravenlic.txt Doc\CmdLine.txt Doc\Beginner.txt Doc\Network.txt Doc\Example.bat Doc\DSS.txt Doc\DHistory.txt Doc\DEDDoc.txt Doc\jDoom\History.txt Doc\jHeretic\History.txt Doc\jHexen\History.txt

REM --- Data files

rar -r a %FILE% Src\*.ico Src\*.bmp %_DataPath%\*.dlo %_DataPath%\*.wad %_DataPath%\Fonts\*.* Data\KeyMaps\*.* %_DataPath%\*Obj.txt %_DataPath%\*.ico Runtime\jDoom\*.rsp Runtime\jHeretic\*.rsp Runtime\jHexen\*.rsp Runtime\jDoom\Startup.cfg Runtime\jHeretic\Startup.cfg Runtime\jHexen\Startup.cfg

REM --- Other files

REM --- rar a %FILE% makedist.bat packcom.bat packsetup.bat makesup.bat makesrc.bat *.txt *.lst %_UtilPath%\*.*
rar a %FILE% %_UtilPath%\*.*

rar c -z..\%THISDIR%\ddsrccom.txt %FILE%.exe

cd ..\%THISDIR%
