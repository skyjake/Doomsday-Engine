REM -- Does a complete Release distribution.

call makedist rel
call packcom e d h x mh mx
call packdist
call packsetup f mh mx
call makesrc
