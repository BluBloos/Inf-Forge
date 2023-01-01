@echo off
SETLOCAL

:: Check if the required number of parameters was passed
if "%~2"=="" (
    echo Insufficient parameters.
    goto :EOF
)

:: Assign the first parameter to the destination variable
set "dest=%~1"
set "dest=%dest:/=\%"
echo dest=%dest%

:: Iterate over the list of file names
for %%f in (%*) do (

    if not "%%~f"=="%dest%" (

        :: Check if the destination file exists
        if exist "%dest%\%%~f" (
            :: Calculate the SHA of the source file
            for /f %%a in ('certutil -hashfile "%%~f" SHA256') do set "hash1=%%a"

            :: Calculate the SHA of the destination file
            for /f %%a in ('certutil -hashfile "%dest%\%%~f" SHA256') do set "hash2=%%a"

            :: Compare the two hashes and copy the file if they are different
            if not "%hash1%"=="%hash2%" (
                copy "%%~f" "%dest%"
                echo Copied "%%~f" to "%dest%".
            )
        ) else (
            :: The destination file does not exist, so just copy the source file
            copy "%%~f" "%dest%"
            echo Copied "%%~f" to "%dest%".
        )
    )
)

ENDLOCAL