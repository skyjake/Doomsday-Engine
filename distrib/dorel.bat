REM -- Does a complete Release distribution.

REM -- Rebuild Docs.
cd ..\doomsday\Doc\Ame
call make h t
cd ..\..\..\distrib

REM -- Recompile.
cd ..\doomsday
call d:\vs.net\vc7\bin\vcvars32.bat
call vcbuild res
call d:\vctk\vcvars32.bat
call vcbuild all
cd ..\distrib

REM -- Run the Inno Setup Compiler.
"C:\Program Files\Inno Setup 5\Compil32.exe" /cc setup.iss

REM call makedist rel
REM call packcom e d h x 
REM call packdist
REM call packsetup f 
call makesrc
