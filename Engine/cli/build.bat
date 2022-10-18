:: TODO(Noah): actually utilize the format_files.bat facility that we have created. The reason that
:: we are not currently using this is that we are not yet satisfied with the formatting rules in all cases.
:: And I feel that getting to the correct format rules is a tedious process.

@echo off
SETLOCAL

if "%1"=="" goto bad2

@echo creating bin dir/
@mkdir bin

:: the use of the @ symbol here makes the command processor show only the output of the command
:: without actually showing the execution of the command.
@set APP_ROOT=%cd%
@set ENGINE_ROOT=%~dp0..
@set ENGINE_EXTERNAL=%ENGINE_ROOT%\..\external

:: Install game resources into the bin dir.
@xcopy "%APP_ROOT%\res" bin\res /E /Y /I

@set INCLUDES=/I "%ENGINE_ROOT%\src" /I "%ENGINE_ROOT%\include" ^
    /I "%APP_ROOT%\include" /I "%ENGINE_ROOT%" /I "%APP_ROOT%" /I "%ENGINE_EXTERNAL%" ^
    /I "%ENGINE_EXTERNAL%\imgui-1.87"
if "%1"=="GL_BACKEND" (
    @set INCLUDES=/I "%ENGINE_EXTERNAL%\imgui-1.87\backends" ^
        /I "%ENGINE_EXTERNAL%\glm-0.9.9.8" ^
        /I "%ENGINE_EXTERNAL%\glew-2.2.0\include\GL" %INCLUDES% ^
        /I "%ENGINE_EXTERNAL%\glm-0.9.9.8"
)

@set SOURCES=
if "%1"=="GL_BACKEND" (
    @set SOURCES="%ENGINE_EXTERNAL%\imgui-1.87\backends\imgui_impl_opengl3.cpp" ^
    "%ENGINE_EXTERNAL%\imgui-1.87\backends\imgui_impl_win32.cpp" %SOURCES%
)

:: NOTE(Noah): Everyone gets imGUI!
FOR %%A IN ("%ENGINE_EXTERNAL%\imgui-1.87\imgui*.cpp") DO call set "SOURCES="%%A" %%SOURCES%%"

:: TODO(Noah): Because we have added /EHsc, stack unwiding and C++ exceptions
:: are supported. Let's make sure to prefix our funcs with extern "C" so that
:: compiler can optimize our funcs (because WE will certainly never throw an
:: exception)
@set CFLAGS=-Zi /D DEBUG /FeAutomataApp /EHsc

if "%1"=="GL_BACKEND" (
    @set CFLAGS=/D GLEW_STATIC /D GL_BACKEND %CFLAGS%
    @set LFLAGS=/link /LIBPATH:"%ENGINE_EXTERNAL%\glew-2.2.0\lib\Release\x64" -incremental:no
)
if "%1"=="CPU_BACKEND" (
    @set CFLAGS=/D CPU_BACKEND %CFLAGS%
)

:: what the heck is going on with this for loop here?
:: call command will invoke another batch program without stopping this batch program
:: that's about all I can say
:: otherwise, ... I have no idea what is going on. I copied from Stack Overflow.
:: and honestly man, I'm fine with that. Maybe someday I will figure out what is going on.
:: https://stackoverflow.com/questions/18600317/batch-file-append-a-string-inside-a-for-loop/18600971#18600971
FOR %%A IN ("%ENGINE_ROOT%\src\*.cpp") DO call set "SOURCES="%%A" %%SOURCES%%"

FOR %%A IN ("%APP_ROOT%\src\*.cpp") DO call set "SOURCES="%%A" %%SOURCES%%"

if "%2"=="" (
    :: noop 
    break
) else (
    :: TODO(Noah): does this even work for just one token? just two?
    FOR /F "tokens=1,2,3 delims=:" %%B in (%2) do (
        echo %%B && FOR %%A IN ("%APP_ROOT%\%%B\*.cpp") DO call set "SOURCES="%%A" %%SOURCES%%"
        echo %%C && FOR %%A IN ("%APP_ROOT%\%%C\*.cpp") DO call set "SOURCES="%%A" %%SOURCES%%"
        echo %%D && FOR %%A IN ("%APP_ROOT%\%%D\*.cpp") DO call set "SOURCES="%%A" %%SOURCES%%"
    )
)

:: run the gist prehook
:: TODO(Noah): Make our prehook be able to take in multiple directory trees.
python "%ENGINE_ROOT%\cli\build.py" "%ENGINE_ROOT%"
python "%ENGINE_ROOT%\cli\build.py" "%APP_ROOT%"

pushd bin

:: TODO: Make non-specific to my host machine.
@echo setting up MSVC compiler
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

if "%1"=="DX12_BACKEND" (
    cl %CFLAGS% %INCLUDES% %SOURCES% gdi32.lib ^
        user32.lib kernel32.lib Xaudio2.lib ole32.lib D3D12.lib DXGI.lib D3DCompiler.lib
)
if "%1"=="GL_BACKEND" (
    cl %CFLAGS% %INCLUDES% %SOURCES% %LFLAGS% gdi32.lib ^
        opengl32.lib user32.lib glew32s.lib kernel32.lib Xaudio2.lib ole32.lib
)
if "%1"=="CPU_BACKEND" (
    cl %CFLAGS% %INCLUDES% %SOURCES% %LFLAGS% gdi32.lib ^
        user32.lib kernel32.lib Xaudio2.lib ole32.lib
)
if "%1"=="VULKAN_BACKEND" (
    cl %CFLAGS% %INCLUDES% %SOURCES% %LFLAGS% gdi32.lib ^
        user32.lib kernel32.lib Xaudio2.lib ole32.lib
)

popd

goto end
:bad2
echo Please provide graphics backend
:end

ENDLOCAL