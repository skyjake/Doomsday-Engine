@ECHO OFF
REM -- This batch file uses Py2exe to create a Windows executable.
REM -- A beta distribution package is compiled by compressing the
REM -- executable, the necessary resource files, and the Snowberry 
REM -- scripts into a self-extracting RAR archive.

rd/s/q dist

REM -- Make the executable.
python setup.py py2exe

REM -- Additional binary dependencies.
copy %PYTHON_DIR%\lib\site-packages\wx-2.8-msw-ansi\wx\msvcp71.dll dist
