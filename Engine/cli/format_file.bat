@echo off
setlocal
clang-format --verbose --ferror-limit=8 --style=file %1 | git diff -b --no-index -- %1 -

:PROMPT
:: /P means set value of variable to user input.
SET /P AREYOUSURE=Commit this formatting change (Y/[N])?
:: /I will ignore case.
IF /I "%AREYOUSURE%" NEQ "Y" GOTO END

:: -i for modify file in place.
clang-format -i --style=file %1

:END
endlocal