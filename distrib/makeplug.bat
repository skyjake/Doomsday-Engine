@ECHO OFF
REM --- Build the plugin SDK.

ECHO ================= MAKING PLUGIN SDK PACKAGE ==================

SET SRC=..\doomsday\
SET OUT=Out\
SET RAR=-s -m5 -sfx

REM --- Make sure destination directory exists.
md %OUT%

REM --- Pack it with RAR.

SET FILE=%OUT%ddplugsdk%1
del %FILE%.exe

REM --- Files

rar -r a %FILE% %SRC%Include\doomsday.h %SRC%Include\dd_share.h %SRC%Include\dd_plugin.h %SRC%Include\dd_types.h %SRC%Include\p_think.h %SRC%Include\dd_dfdat.h %SRC%Include\dglib.h %SRC%Include\Common\g_dgl.h %SRC%Src\Common\g_dglinit.c
rar -r -ep a %FILE% %SRC%Bin\Release\Doomsday.lib

echo Uploading to The Mirror...
ftpscrpt -f z:\scripts\mirror-plugsdk.scp

echo Uploading to Fourwinds...
ftpscrpt -f z:\scripts\plugsdk.scp
