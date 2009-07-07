@ECHO OFF
ECHO Setting up environment for SDKs...

SET SDLDIR=d:\SDK\SDL-1.2.13
SET SDLMIXERDIR=d:\SDK\SDL_mixer-1.2.8
SET SDLNETDIR=d:\SDK\SDL_net-1.2.7
SET PLATFORMSDKDIR=C:\Program Files\Microsoft SDKs\Windows\v6.0A
IF NOT "%DXSDK_DIR%" == "" SET DXDIR=%DXSDK_DIR%
IF "%DXSDK_DIR%" == "" SET DXDIR=d:\SDK\DirectX (March 2009)
