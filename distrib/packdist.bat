@ECHO OFF
REM -- Packs the deng binary ZIP package.

SET FILE=..\Out\deng-1.zip

cd distrib

del %FILE%
wzzip -a -ex -P %FILE% *.kss *.txt *.exe 
wzzip -a -ex -r -P %FILE% Run\*.* Doc\*.* Data\*.* Bin\*.*
wzzip -a -ex -P -x@..\mdefex.lst %FILE% Defs\jDoom\*.ded Defs\jDoom\Auto\*.*
wzzip -a -ex -P -x@..\mdefex.lst %FILE% Defs\jHeretic\*.ded Defs\jHeretic\Auto\*.*
wzzip -a -ex -P -x@..\mdefex.lst %FILE% Defs\jHexen\*.ded Defs\jHexen\Auto\*.*

cd ..