REM -- Does a complete Release distribution.

REM -- Run the Inno Setup Compiler.
"C:\Program Files\Inno Setup 5\Compil32.exe" /cc setup.iss

REM call makedist rel
REM call packcom e d h x 
REM call packdist
REM call packsetup f 
call makesrc
