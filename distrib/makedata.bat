@ECHO OFF
REM --- Build the DATA package.
REM --- Includes support libraries, tools, graphics and WADs.

ECHO ========================== MAKING SOURCE PACKAGE ===========================

REM -- Prepare output directory.
md Out
SET THISDIR=distrib
SET FILE=..\%THISDIR%\Out\deng-data-1.zip

cd ..\doomsday

REM --- Pack it with WZZIP.

del %FILE%

wzzip -a -ex -P -r %FILE% Data\*.ico Data\*.dlo Data\Graphics\*.* Data\Doomsday.wad Data\Fonts\*.* Data\j*.wad Data\*.cdm DLLs\*.* Lib\*.* Utils\*.* 

cd ..\%THISDIR%
