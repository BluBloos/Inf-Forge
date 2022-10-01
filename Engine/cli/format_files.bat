:: this file is a utility to format a source project. It does not modify source files.
:: rather, it outputs on cmd that your files are borked and not adhering to the format
:: guidelines!
@echo off
SETLOCAL

@set ROOT_DIR=%1
@set SOURCES=

:: %~dp0 is dir of this .bat
FOR /R %ROOT_DIR% %%A IN ("*.cpp") DO call "%~dp0\format_file.bat" "%%A"

ENDLOCAL