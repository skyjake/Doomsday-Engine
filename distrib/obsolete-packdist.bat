@ECHO OFF
REM -- Create the jDoom, jHeretic, jHexen distributions.
REM -- Also creates Model Packs for all three, and a unified
REM -- full Doomsday distribution. The component targets are
REM -- meant for partial Patch distributions.

set _PackJD=No
set _PackJH=No
set _PackJX=No
set _PackJDMP=No
set _PackJHMP=No
set _PackJXMP=No
set _PackJDMPDef=No
set _PackJHMPDef=No
set _PackJXMPDef=No
set _PackFull=No
set _PackFullMP=No
set _PackCompEng=No
set _PackCompJD=No
set _PackCompJH=No
set _PackCompJX=No

:beginloop
if "%1" == "" goto start_packing
if "%1" == "a" goto set_all
if "%1" == "d" set _PackJD=Yes
if "%1" == "h" set _PackJH=Yes
if "%1" == "x" set _PackJX=Yes
if "%1" == "f" set _PackFull=Yes
if "%1" == "md" set _PackJDMP=Yes
if "%1" == "mh" set _PackJHMP=Yes
if "%1" == "mx" set _PackJXMP=Yes
if "%1" == "mdd" set _PackJDMPDef=Yes
if "%1" == "mdh" set _PackJHMPDef=Yes
if "%1" == "mdx" set _PackJXMPDef=Yes
if "%1" == "mf" set _PackFullMP=Yes
if "%1" == "ce" set _PackCompEng=Yes
if "%1" == "cd" set _PackCompJD=Yes
if "%1" == "ch" set _PackCompJH=Yes
if "%1" == "cx" set _PackCompJX=Yes

shift
goto beginloop

:set_all
set _PackJD=Yes
set _PackJH=Yes
set _PackJX=Yes
set _PackFull=Yes
set _PackJDMP=Yes
set _PackJHMP=Yes
set _PackJXMP=Yes
set _PackJDMPDef=Yes
set _PackJHMPDef=Yes
set _PackJXMPDef=Yes
set _PackFullMP=Yes
set _PackCompEng=Yes
set _PackCompJD=Yes
set _PackCompJH=Yes
set _PackCompJX=Yes
shift
goto beginloop

:start_packing
set RAR=-s -m5 -sfx

REM +---------------------------------------------------------------+
REM + Engine Component						    +
REM +---------------------------------------------------------------+

if %_PackCompEng%==No goto skip_ceng
set FILE=out\patch_deng1070
del %FILE%.exe
REM rar a %FILE% -r y:\Defs\jDoom -x@mdefex.lst
rar a %FILE% y:\glbsp.* y:\Doc\*.* y:\Bin\*.* y:\Data\*.* y:\Data\Fonts y:\Data\KeyMaps -xBin\jDoom.dll -x*heretic*.* -x*hexen*.* -xdeath.kss
rar c -zengcom.txt %FILE%.exe
:skip_ceng

REM +---------------------------------------------------------------+
REM + jDoom	  						    +
REM +---------------------------------------------------------------+

if %_PackJD%==No goto skip_jd
set FILE=out\jdoom1140
del %FILE%.exe
rar a %FILE% -r y:\Defs\jDoom -x@mdefex.lst
rar a %FILE% y:\*.* y:\Doc\*.* y:\Doc\jDoom\*.* y:\Bin\*.* y:\Run\jDoom y:\Data\*.* y:\Data\jDoom y:\Data\Fonts y:\Data\KeyMaps -x*heretic*.* -x*hexen*.* -xdeath.kss
rar c -zjdcom.txt %FILE%.exe
:skip_jd

if %_PackCompJD%==No goto skip_cjd
set FILE=out\patch_jdoom1140
del %FILE%.exe
rar a %FILE% -r y:\Defs\jDoom -x@mdefex.lst
rar a %FILE% y:\Kicks.exe y:\*.kss y:\jDoom.exe y:\Doc\jDoom\*.* y:\Bin\jDoom.dll y:\Run\jDoom y:\Data\jDoom -x*heretic*.* -x*hexen*.* -xdeath.kss
rar c -zcjdcom.txt %FILE%.exe
:skip_cjd

if %_PackJDMP%==No goto skip_jdmp
del out\jdoom_mpack.exe
rar a out\jdoom_mpack -r y:\Md2\jDoom @jdmdefs.lst
rar c -zjdmcom.txt out\jdoom_mpack.exe
:skip_jdmp

if %_PackJDMPDef%==No goto skipddef
del out\jdoom_mpack_defs.exe
rar a out\jdoom_mpack_defs @jdmdefs.lst
rar c -zjdmdefcom.txt out\jdoom_mpack_defs.exe
:skipddef

REM +---------------------------------------------------------------+
REM + jHeretic	  						    +
REM +---------------------------------------------------------------+

if %_PackJH%==No goto skip_jh
set FILE=out\jheretic1030
del %FILE%.exe
rar a %FILE% -r y:\Defs\jHeretic -x@mdefex.lst y:\Heretic.kss
rar a %FILE% y:\*.* y:\Doc\*.* y:\Doc\jHeretic\*.* y:\Bin\*.* y:\Run\jHeretic y:\Data\*.* y:\Data\jHeretic y:\Data\Fonts y:\Data\KeyMaps -x*jdoom*.* -x*hexen*.* -x*.kss
rar c -zjhcom.txt %FILE%.exe
:skip_jh

if %_PackCompJH%==No goto skip_cjh
set FILE=out\patch_jheretic1030
del %FILE%.exe
rar a %FILE% -r y:\Defs\jHeretic -x@mdefex.lst y:\Heretic.kss
rar a %FILE% y:\Kicks.exe y:\jHeretic.exe y:\Doc\jHeretic\*.* y:\Run\jHeretic y:\Bin\jHeretic.dll y:\Data\jHeretic -xjdoom*.* -x*hexen*.*
rar c -zcjhcom.txt %FILE%.exe
:skip_cjh

if %_PackJHMP%==No goto skip_jhmp
del out\jheretic_mpack.exe
rar a out\jheretic_mpack -r y:\Md2\jHeretic @jhmdefs.lst
rar c -zjhmcom.txt out\jheretic_mpack.exe
:skip_jhmp

if %_PackJHMPDef%==No goto skiphdef
del out\jheretic_mpack_defs.exe
rar a out\jheretic_mpack_defs @jhmdefs.lst
rar c -zjhmdefcom.txt out\jheretic_mpack_defs.exe
:skiphdef

REM +---------------------------------------------------------------+
REM + jHexen	  						    +
REM +---------------------------------------------------------------+

if %_PackJX%==No goto skip_jx
set FILE=out\jhexen1020
del %FILE%.exe
rar a %FILE% -r y:\Defs\jHexen -x@mdefex.lst y:\Hexen.kss y:\Death.kss
rar a %FILE% y:\*.* y:\Doc\*.* y:\Doc\jHexen\*.* y:\Bin\*.* y:\Run\jHexen y:\Data\*.* y:\Data\jHexen y:\Data\Fonts y:\Data\KeyMaps -x*jdoom*.* -x*heretic*.* -x*.kss
rar c -zjxcom.txt %FILE%.exe
:skip_jx

if %_PackCompJX%==No goto skip_cjx
set FILE=out\patch_jhexen1020
del %FILE%.exe
rar a %FILE% -r y:\Defs\jHexen -x@mdefex.lst y:\Hexen.kss y:\Death.kss
rar a %FILE% y:\Kicks.exe y:\jHexen.exe y:\Doc\jHexen\*.* y:\Bin\jHexen.dll y:\Data\jHexen y:\Run\jHexen
rar c -zcjxcom.txt %FILE%.exe
:skip_cjx

if %_PackJXMP%==No goto skip_jxmp
del out\jhexen_mpack.exe
rar a out\jhexen_mpack -r y:\Md2\jHexen @jxmdefs.lst
rar c -zjxmcom.txt out\jhexen_mpack.exe
:skip_jxmp

if %_PackJXMPDef%==No goto skipxdef
del out\jhexen_mpack_defs.exe
rar a out\jhexen_mpack_defs @jxmdefs.lst
rar c -zjxmdefcom.txt out\jhexen_mpack_defs.exe
:skipxdef

REM +---------------------------------------------------------------+
REM + Full 							    +
REM +---------------------------------------------------------------+

if %_PackFull%==No goto skip_full
set FILE=out\doomsday1070
del %FILE%.exe
rar a %FILE% y:\*.* -x*.~_~
rar a %FILE% -r y:\Defs -x@mdefex.lst y:\Doc y:\Run y:\Bin y:\Data
rar c -zddcom.txt %FILE%.exe
:skip_full

if %_PackFullMP%==No goto skip_fmp
del out\doomsday_mpack.exe
rar a out\doomsday_mpack -r y:\Md2
rar a out\doomsday_mpack @jdmdefs.lst @jhmdefs.lst @jxmdefs.lst
rar c -zddmcom.txt out\doomsday_mpack.exe
:skip_fmp
