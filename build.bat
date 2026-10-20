@echo off
rem Build legacy_lgd.sln (Release, Win32). Executables go to bin\Win32\Release\.
rem Change MSBUILD_EXE if Visual Studio is installed elsewhere, or just open the .sln in Visual Studio.
rem (Comments in .bat files are kept in English: cmd parses UTF-8 Chinese text incorrectly.)

set MSBUILD_EXE="D:\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"

cd /d "%~dp0"
%MSBUILD_EXE% legacy_lgd.sln /p:Configuration=Release /p:Platform=Win32 /m /nologo /v:minimal || exit /b 1

echo Build OK
@pause
