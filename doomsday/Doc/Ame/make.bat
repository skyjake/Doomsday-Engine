@echo off

:check
IF "%1" == "h" GOTO html
IF "%1" == "t" GOTO txt
IF "%1" == "" GOTO end
:next
shift
goto check
        
:html
for %%f in (*.ame) do amethyst -dHTML -ic:\amethyst -eHTML -ohtml\%%f %%f
goto next

:txt
for %%f in (*.ame) do amethyst -dTXT -ic:\amethyst -eTXT -otxt\%%f %%f
goto next

:end

