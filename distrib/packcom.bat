@ECHO OFF
REM -- Create the Marine Installer Components. 

md Setup

set COMMENT=..\mic_com.txt
set SRC=.
set _DestDir=..\Setup
set _PackJD=No
set _PackJH=No
set _PackJX=No
set _PackJDMP=No
set _PackJHMP=No
set _PackJXMP=No
set _PackEng=No

:beginloop
if "%1" == "" goto start_packing
if "%1" == "a" goto set_all
if "%1" == "e" set _PackEng=Yes
if "%1" == "d" set _PackJD=Yes
if "%1" == "h" set _PackJH=Yes
if "%1" == "x" set _PackJX=Yes
if "%1" == "md" set _PackJDMP=Yes
if "%1" == "mh" set _PackJHMP=Yes
if "%1" == "mx" set _PackJXMP=Yes

shift
goto beginloop

:set_all
set _PackEng=Yes
set _PackJD=Yes
set _PackJH=Yes
set _PackJX=Yes
set _PackJDMP=Yes
set _PackJHMP=Yes
set _PackJXMP=Yes
shift
goto beginloop

:start_packing
set RAR=-s -m0 -sfx
cd Distrib

REM +---------------------------------------------------------------+
REM + Engine Component						    +
REM +---------------------------------------------------------------+

if %_PackEng%==No goto skip_eng
set FILE=%_DestDir%\_mic_engine
del %FILE%.exe
rar a %FILE% %SRC%\Kicks.exe %SRC%\*.cfg %SRC%\*.ksl %SRC%\glbsp.* %SRC%\Doc\*.* %SRC%\Bin\*.* %SRC%\Data\*.* %SRC%\Data\Fonts %SRC%\Data\KeyMaps -xBin\jDoom.dll -x*heretic*.* -x*hexen*.*
rar c -z%COMMENT% %FILE%.exe
:skip_eng

REM +---------------------------------------------------------------+
REM + jDoom	  						    +
REM +---------------------------------------------------------------+

if %_PackJD%==No goto skip_cjd
set FILE=%_DestDir%\_mic_jdoom
del %FILE%.exe
rar a %FILE% -r %SRC%\Defs\jDoom -x@..\mdefex.lst
rar a %FILE% %SRC%\*.ksp %SRC%\jDoom.exe %SRC%\Doc\jDoom\*.* %SRC%\Bin\jDoom.dll %SRC%\Run\jDoom %SRC%\Data\jDoom -x*heretic*.* -x*hexen*.* -xdeath.ksp
rar c -z%COMMENT% %FILE%.exe
:skip_cjd

if %_PackJDMP%==No goto skip_jdmp
set FILE=%_DestDir%\_mic_jdoom_mpack
del %FILE%.exe
rar a %FILE% -r %SRC%\Md2\jDoom @..\jdmdefs.lst
rar c -z%COMMENT% %FILE%.exe
:skip_jdmp

REM +---------------------------------------------------------------+
REM + jHeretic	  						    +
REM +---------------------------------------------------------------+

if %_PackJH%==No goto skip_cjh
set FILE=%_DestDir%\_mic_jheretic
del %FILE%.exe
rar a %FILE% -r %SRC%\Defs\jHeretic -x@..\mdefex.lst %SRC%\Heretic.ksp
rar a %FILE% %SRC%\jHeretic.exe %SRC%\Doc\jHeretic\*.* %SRC%\Run\jHeretic %SRC%\Bin\jHeretic.dll %SRC%\Data\jHeretic -xjdoom*.* -x*hexen*.*
rar c -z%COMMENT% %FILE%.exe
:skip_cjh

if %_PackJHMP%==No goto skip_jhmp
set FILE=%_DestDir%\_mic_jheretic_mpack
del %FILE%.exe
rar a %FILE% -r -x*.dmd %SRC%\Md2\jHeretic @..\jhmdefs.lst 
rar c -z%COMMENT% %FILE%.exe
:skip_jhmp

REM +---------------------------------------------------------------+
REM + jHexen	  						    +
REM +---------------------------------------------------------------+

if %_PackJX%==No goto skip_cjx
set FILE=%_DestDir%\_mic_jhexen
del %FILE%.exe
rar a %FILE% -r %SRC%\Defs\jHexen -x@..\mdefex.lst %SRC%\Hexen.ksp %SRC%\Death.ksp
rar a %FILE% %SRC%\jHexen.exe %SRC%\Doc\jHexen\*.* %SRC%\Bin\jHexen.dll %SRC%\Data\jHexen %SRC%\Run\jHexen
rar c -z%COMMENT% %FILE%.exe
:skip_cjx

if %_PackJXMP%==No goto skip_jxmp
set FILE=%_DestDir%\_mic_jhexen_mpack
del %FILE%.exe
rar a %FILE% -r -x*.dmd %SRC%\Md2\jHexen @..\jxmdefs.lst
rar c -z%COMMENT% %FILE%.exe
:skip_jxmp

cd ..
