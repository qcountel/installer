@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo Building Minecraft Installer...

REM Find Visual Studio (2019/2022/2026) with C++ tools
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"

if "%VS_PATH%"=="" (
    echo Visual Studio with "Desktop development with C++" not found!
    pause
    exit /b 1
)

set "CMAKE_EXE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE_EXE%" set "CMAKE_EXE=cmake"

if exist build rd /s /q build

echo Configuring...
"%CMAKE_EXE%" -S . -B build -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 > build.log 2>&1
if errorlevel 1 goto fail

echo Building (Release)... first build downloads and compiles wxWidgets, it takes a while.
"%CMAKE_EXE%" --build build --config Release --parallel >> build.log 2>&1
if errorlevel 1 goto fail

echo.
echo Done: build\Release\Minecraft Installer.exe
pause
exit /b 0

:fail
echo Build failed, see build.log
pause
exit /b 1
