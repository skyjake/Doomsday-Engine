@ECHO OFF
REM -- Makes the Doomsday distribution in the Distrib subdir.

SET COPYCMD=/Y
SET Dist=Distrib\
SET DistBin=%Dist%\Bin\

REM -- Source directories (where files will be copied from).
SET SRC=..\doomsday\
SET FMODLIB=D:\sdk\fmod\api\
SET SidgarDir=C:\Projects\SiDGaR\Release\
SET KickDir=..\kickstart\
SET BSPDir=c:\utils\glbsp\

IF "%1" == "rel" GOTO releasedirs
IF "%1" == "beta" GOTO betadirs

ECHO ===================== MAKING  D E B U G  DISTRIBUTION =====================
SET BinDir=%SRC%Bin\Debug\
SET SupDir=%SRC%Bin\Debug\
GOTO startcopy

:betadirs
ECHO ====================== MAKING  B E T A  DISTRIBUTION ======================
SET BinDir=%SRC%Bin\Debug\
SET SupDir=%SRC%Bin\Release\
GOTO startcopy

:releasedirs
ECHO =================== MAKING  R E L E A S E  DISTRIBUTION ===================
SET BinDir=%SRC%Bin\Release\
SET SupDir=%SRC%Bin\Release\

:startcopy

REM +---------------------------------------------------------------+
REM + Binaries							    +
REM +---------------------------------------------------------------+
echo Copying binaries...

xcopy %BinDir%Doomsday.exe	%DistBin%
xcopy %BinDir%jDoom.dll 	%DistBin%
xcopy %BinDir%jHeretic.dll 	%DistBin%
xcopy %BinDir%jHexen.dll 	%DistBin%
xcopy %SupDir%drOpenGL.dll 	%DistBin%
xcopy %SupDir%drD3D.dll 	%DistBin%
xcopy %SupDir%dsA3D.dll		%DistBin%
xcopy %SupDir%dsCompat.dll	%DistBin%
xcopy %SupDir%dpDehRead.dll     %DistBin%
xcopy %SRC%DLLs\LZSS.dll 	%DistBin%
xcopy %SRC%DLLs\EAX.dll 	%DistBin%
xcopy %SRC%DLLs\zlib.dll	%DistBin%
xcopy %FMODLIB%fmod.dll		%DistBin%
xcopy %SidgarDir%*.exe		%Dist%

REM +---------------------------------------------------------------+
REM + Data							    +
REM +---------------------------------------------------------------+
echo Copying Data...

xcopy %SRC%Data\Doomsday.wad		%Dist%Data\
xcopy %SRC%Data\CPHelp.txt		%Dist%Data\
xcopy %SRC%Data\jDoom\jDoom.wad		%Dist%Data\jDoom\
xcopy %SRC%Data\jDoom\udemo2.cdm	%Dist%Run\jDoom\Demo\
xcopy %SRC%Data\jHeretic\jHeretic.wad	%Dist%Data\jHeretic\
xcopy %SRC%Data\jHexen\jHexen.wad	%Dist%Data\jHexen\
xcopy %SRC%Data\Fonts\*.dfn		%Dist%Data\Fonts\
xcopy %SRC%Data\Fonts\Readme.txt	%Dist%Data\Fonts\
xcopy %SRC%Data\KeyMaps\*.dkm		%Dist%Data\KeyMaps\
xcopy %SRC%Data\Graphics\*.*		%Dist%Data\Graphics\
xcopy %SRC%Data\jDoom\Auto\Readme.txt	%Dist%Data\jDoom\Auto\
xcopy %SRC%Data\jHeretic\Auto\Readme.txt %Dist%Data\jHeretic\Auto\
xcopy %SRC%Data\jHexen\Auto\Readme.txt	%Dist%Data\jHexen\Auto\

REM +---------------------------------------------------------------+
REM + Definitions						    +
REM +---------------------------------------------------------------+
echo Copying Definitions...

xcopy /s %SRC%Defs\jDoom\*.ded		%Dist%Defs\jDoom\
xcopy /s %SRC%Defs\jHeretic\*.ded	%Dist%Defs\jHeretic\
xcopy /s %SRC%Defs\jHexen\*.ded		%Dist%Defs\jHexen\
xcopy /s %SRC%Defs\jDoom\Auto\*.txt	%Dist%Defs\jDoom\Auto\
xcopy /s %SRC%Defs\jHeretic\Auto\*.txt	%Dist%Defs\jHeretic\Auto\
xcopy /s %SRC%Defs\jHexen\Auto\*.txt	%Dist%Defs\jHexen\Auto\

REM +---------------------------------------------------------------+
REM + Scripts    						    +
REM +---------------------------------------------------------------+
echo Copying Scripts...

xcopy %SRC%Runtime\jDoom\Startup.cfg	%Dist%Run\jDoom\
xcopy %SRC%Runtime\jHeretic\Startup.cfg %Dist%Run\jHeretic\
xcopy %SRC%Runtime\jHexen\Startup.cfg	%Dist%Run\jHexen\

REM +---------------------------------------------------------------+
REM + Model Files						    +
REM +---------------------------------------------------------------+
echo Copying Models (not jDoom's)...

REM -- deltree /y %Dist%\Md2\jDoom
rd /s /q %Dist%Md2\jHeretic
rd /s /q %Dist%Md2\jHexen

REM -- xcopy /e /s Md2\jDoom\*.*       %Dist%\Md2\jDoom\
xcopy /y /e /s %SRC%Md2\jHeretic\*.*    %Dist%Md2\jHeretic\
xcopy /y /e /s %SRC%Md2\jHexen\*.*      %Dist%Md2\jHexen\

REM +---------------------------------------------------------------+
REM + Documentation						    +
REM +---------------------------------------------------------------+
echo Copying Documentation...

xcopy %SRC%Doc\ChangeLog.txt           	%Dist%Doc\
xcopy %SRC%Doc\Ame\TXT\Readme.txt	%Dist%Doc\
xcopy %SRC%Doc\Ame\TXT\Beginner.txt	%Dist%Doc\
xcopy %SRC%Doc\Ame\TXT\CmdLine.txt	%Dist%Doc\
ren %Dist%Doc\Readme.txt 	Readme.txt 
ren %Dist%Doc\Beginner.txt 	Beginner.txt 
ren %Dist%Doc\CmdLine.txt 	CmdLine.txt 
xcopy %SRC%Doc\DEDDoc.txt		%Dist%Doc\
xcopy %SRC%Doc\DSS.txt			%Dist%Doc\
xcopy %SRC%Doc\Network.txt		%Dist%Doc\
xcopy %SRC%Doc\Example.bat            	%Dist%Doc\
xcopy %SRC%Doc\InFine.txt		%Dist%Doc\
xcopy %SRC%Doc\jDoom\Doomlic.txt      	%Dist%Doc\jDoom\
xcopy %SRC%Doc\jDoom\jDoom.txt		%Dist%Doc\jDoom\
xcopy %SRC%Doc\jHeretic\jHeretic.txt	%Dist%Doc\jHeretic\
xcopy %SRC%Doc\Ravenlic.txt		%Dist%Doc\jHeretic\
xcopy %SRC%Doc\jHexen\jHexen.txt	%Dist%Doc\jHexen\
xcopy %SRC%Doc\Ravenlic.txt		%Dist%Doc\jHexen\

REM +---------------------------------------------------------------+
REM + Other Files  						    +
REM +---------------------------------------------------------------+
echo Copying Other Files...

xcopy %KickDir%Kicks.exe 	%Dist%
xcopy %KickDir%*.ks?   		%Dist%
xcopy %KickDir%*.cfg   		%Dist%
xcopy %BSPDir%glbsp.exe   	%Dist%
xcopy %BSPDir%glbsp.txt   	%Dist%

