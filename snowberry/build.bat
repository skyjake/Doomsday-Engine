@ECHO OFF
REM -- This batch file uses Py2exe to create a Windows executable.
REM -- A beta distribution package is compiled by compressing the
REM -- executable, the necessary resource files, and the Snowberry 
REM -- scripts into a self-extracting RAR archive.

SET RAR=-s -m5 -sfx
SET PACKAGE=snowberry-beta

REM -- Make the executable.
python setup.py py2exe

REM -- Don't build a separate package.
REM del %PACKAGE%.exe

REM rar a %PACKAGE% -ep dist\*.pyd dist\*.dll dist\*.exe dist\*.ico dist\*.zip
REM rar a %PACKAGE% -r -x*\CVS* -x*.pyc -x*~ -xobserver.py addons\README addons\example.addon conf\*.conf graphics\*.bmp graphics\*.png graphics\*.jpg lang\*.lang plugins\example.plugin plugins\*.py profiles\README profiles\*.prof
