@echo off
rem One-click launcher. The build copies this file next to every compiler exe and renames it
rem to match (legacy_lgd_compiler_as1.bat -> legacy_lgd_compiler_as1.exe, and so on).
rem
rem   No arguments  : compile every map script in .\maps into .\maps with DEFAULT_OPTIONS
rem                   (like the official lgc_compiler.bat), then pause.
rem                   Run it from the game root, or double-click it there.
rem   With arguments: forwarded unchanged to the exe; the options are the same as the official
rem                   lgc_compiler.exe: [--source-dir=DIR] [--target-dir=DIR] [--game-root=DIR]
rem                   [--stop-on-error] [FILES...]
rem
rem (Comments in .bat files are kept in English: cmd parses UTF-8 Chinese text incorrectly.)

setlocal

rem Extra options for the no-argument mode (the official lgc_compiler.bat also stops on the first error).
rem Clear it to keep going after a failed file.
set "DEFAULT_OPTIONS=--stop-on-error"

rem The exe has the same name as this bat and sits in the same directory
set "EXE=%~dpn0.exe"
if not exist "%EXE%" (
    echo [launcher] compiler not found: %EXE%
    exit /b 2
)

if "%~1"=="" goto compile_maps

rem %* keeps "--source-dir=C:\maps" intact (%1 %2 ... would split it at the '=' sign)
"%EXE%" %*
exit /b %ERRORLEVEL%

:compile_maps
set "MAPS_DIR=%CD%\maps"
"%EXE%" "--source-dir=%MAPS_DIR%" "--target-dir=%MAPS_DIR%" %DEFAULT_OPTIONS%
set "RESULT=%ERRORLEVEL%"
pause
exit /b %RESULT%
