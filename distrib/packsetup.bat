@ECHO OFF
REM -- Create the jDoom, jHeretic, jHexen distributions.
REM -- Also creates Model Packs for all three, and a unified
REM -- full Doomsday distribution. 

REM -- The Setup directory must already contain the components
REM -- created using packcom.bat.

REM -- Get the Marine Installer.
copy ..\doomsday\Setup\Setup.exe Setup

md Out
set _DestDir=Out
set _PackDir=Setup
set _PackEng=No
set _PackJD=No
set _PackJH=No
set _PackJX=No
set _PackJDMP=No
set _PackJHMP=No
set _PackJXMP=No
set _PackFull=No

:beginloop
if "%1" == "" goto start_packing
if "%1" == "a" goto set_all
if "%1" == "e" set _PackEng=Yes
if "%1" == "d" set _PackJD=Yes
if "%1" == "h" set _PackJH=Yes
if "%1" == "x" set _PackJX=Yes
if "%1" == "f" set _PackFull=Yes
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
set _PackFull=Yes
set _PackJDMP=Yes
set _PackJHMP=Yes
set _PackJXMP=Yes
shift
goto beginloop

:start_packing
set RAR=-s -m5 -sfx -ep

REM +---------------------------------------------------------------+
REM + jDoom	  						    +
REM +---------------------------------------------------------------+

if %_PackJD%==No goto skip_jd
set FILE=%_DestDir%\jdoom1140
del %FILE%.exe
rar a %FILE% %_PackDir%\setup.exe
rar c -zsetupcom.txt %FILE%.exe
rar a %FILE% %_PackDir%\deng.mic %_PackDir%\_mic_engine.exe %_PackDir%\jdoom.mic %_PackDir%\_mic_jdoom.exe
:skip_jd

if %_PackJDMP%==No goto skip_jdmp
set FILE=%_DestDir%\jdoom_mpack
del %FILE%.exe
rar a %FILE% %_PackDir%\setup.exe
rar c -zsetupcom.txt %FILE%.exe
rar a %FILE% %_PackDir%\dmpack.mic %_PackDir%\_mic_jdoom_mpack.exe
:skip_jdmp

REM +---------------------------------------------------------------+
REM + jHeretic	  						    +
REM +---------------------------------------------------------------+

if %_PackJH%==No goto skip_jh
set FILE=%_DestDir%\jheretic1030
del %FILE%.exe
rar a %FILE% %_PackDir%\setup.exe
rar c -zsetupcom.txt %FILE%.exe
rar a %FILE% %_PackDir%\deng.mic %_PackDir%\_mic_engine.exe %_PackDir%\jheretic.mic %_PackDir%\_mic_jheretic.exe
:skip_jh

if %_PackJHMP%==No goto skip_jhmp
set FILE=%_DestDir%\jheretic_mpack
del %FILE%.exe
rar a %FILE% %_PackDir%\setup.exe
rar c -zsetupcom.txt %FILE%.exe
rar a %FILE% %_PackDir%\hmpack.mic %_PackDir%\_mic_jheretic_mpack.exe
:skip_jhmp

REM +---------------------------------------------------------------+
REM + jHexen	  						    +
REM +---------------------------------------------------------------+

if %_PackJX%==No goto skip_jx
set FILE=%_DestDir%\jhexen1020
del %FILE%.exe
rar a %FILE% %_PackDir%\setup.exe
rar c -zsetupcom.txt %FILE%.exe
rar a %FILE% %_PackDir%\deng.mic %_PackDir%\_mic_engine.exe %_PackDir%\jhexen.mic %_PackDir%\_mic_jhexen.exe
:skip_jx

if %_PackJXMP%==No goto skip_jxmp
set FILE=%_DestDir%\jhexen_mpack
del %FILE%.exe
rar a %FILE% %_PackDir%\setup.exe
rar c -zsetupcom.txt %FILE%.exe
rar a %FILE% %_PackDir%\xmpack.mic %_PackDir%\_mic_jhexen_mpack.exe
:skip_jxmp

REM +---------------------------------------------------------------+
REM + Full 							    +
REM +---------------------------------------------------------------+

if %_PackEng%==No goto skip_eng
set FILE=%_DestDir%\patch_deng1074
del %FILE%.exe
rar a %FILE% %_PackDir%\setup.exe
rar c -zsetupcom.txt %FILE%.exe
rar a %FILE% %_PackDir%\deng.mic %_PackDir%\_mic_engine.exe
:skip_eng

if %_PackFull%==No goto skip_full
set FILE=%_DestDir%\deng-inst-1
del %FILE%.exe
rar a %FILE% %_PackDir%\setup.exe
rar c -zsetupcom.txt %FILE%.exe
rar a %FILE% %_PackDir%\deng.mic %_PackDir%\_mic_engine.exe %_PackDir%\jdoom.mic %_PackDir%\_mic_jdoom.exe %_PackDir%\jheretic.mic %_PackDir%\_mic_jheretic.exe %_PackDir%\jhexen.mic %_PackDir%\_mic_jhexen.exe
:skip_full

set RAR=