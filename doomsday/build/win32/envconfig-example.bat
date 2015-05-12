@echo off
:: Sets up the command line environment for building.
:: This is automatically called by dorel.bat.

:: Directory Config -------------------------------------------------------

set ENVCONFIG_DIR=%CD%

:: Modify these paths for your system.
set MSVC_DIR=c:\Program Files\Microsoft Visual Studio 12.0
set QTCREATOR_DIR=c:\Qt\Qt5.3.1\Tools\QtCreator
set QT_BIN_DIR=C:\Qt\Qt5.3.1\5.3\msvc2013_opengl\bin

:: Build Tools Setup ------------------------------------------------------

:: Visual C++ environment.
call "%MSVC_DIR%\vc\vcvarsall.bat"

:: -- Qt environment.
set JOM=%QTCREATOR_DIR%\bin\jom.exe /nologo
call "%QT_BIN_DIR%\qtenv2.bat"

cd %ENVCONFIG_DIR%
