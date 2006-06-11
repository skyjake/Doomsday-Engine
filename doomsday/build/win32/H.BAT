@echo off

if "%1x"=="x" goto badargs
if "%2x"=="x" goto badargs

set hticargs=-noartiskip -warp %1 %2
set hticwads=-file
set hticnplay=

:parseloop
if "%3x"=="x" goto doneparse
if "%3"=="m" goto nomonsters
if "%3"=="M" goto nomonsters
if "%3"=="s1" goto skill1
if "%3"=="S1" goto skill1
if "%3"=="s2" goto skill2
if "%3"=="S2" goto skill2
if "%3"=="s3" goto skill3
if "%3"=="S3" goto skill3
if "%3"=="s4" goto skill4
if "%3"=="S4" goto skill4
if "%3"=="s5" goto skill5
if "%3"=="S5" goto skill5
if "%3"=="n1" goto nplay1
if "%3"=="N1" goto nplay1
if "%3"=="n2" goto nplay2
if "%3"=="N2" goto nplay2
if "%3"=="n3" goto nplay3
if "%3"=="N3" goto nplay3
if "%3"=="n4" goto nplay4
if "%3"=="N4" goto nplay4
if "%3"=="p" goto altport
if "%3"=="P" goto altport
goto addwad

:nomonsters
set hticargs=%hticargs% -nomonsters
shift
goto parseloop

:skill1
set hticargs=%hticargs% -skill 1
shift
goto parseloop

:skill2
set hticargs=%hticargs% -skill 2
shift
goto parseloop

:skill3
set hticargs=%hticargs% -skill 3
shift
goto parseloop

:skill4
set hticargs=%hticargs% -skill 4
shift
goto parseloop

:skill5
set hticargs=%hticargs% -skill 5
shift
goto parseloop

:nplay1
set hticnplay=1
shift
goto parseloop

:nplay2
set hticnplay=2
shift
goto parseloop

:nplay3
set hticnplay=3
shift
goto parseloop

:nplay4
set hticnplay=4
shift
goto parseloop

:altport
set hticargs=%hticargs% -port 3
shift
goto parseloop

:addwad
set hticwads=%hticwads% %3.wad
shift
goto parseloop

:badargs
echo Usage: H episode map [s?] [m] [n?] [p] [wadfile [wadfile ...] ]
echo.
echo      [s?] = skill (1-5)
echo       [m] = no monsters
echo      [n?] = net play (1-4)
echo       [p] = use alternate port setting
echo [wadfile] = add external wadfile (.WAD is implicit)
echo.
goto end

:doneparse
if"%hticwads%"=="-file" goto startgame
set hticargs=%hticargs% %hticwads%
:startgame
if"%hticnplay%x"=="x" goto normalplay
echo -nodes %hticnplay% -deathmatch %hticargs%
ipxsetup -nodes %hticnplay% -deathmatch %hticargs%
goto end

:normalplay
echo %hticargs%
heretic %hticargs%
goto end

:end
set hticargs=
set hticwads=
set hticnplay=
