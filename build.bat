@echo off

echo ======================================================================
echo             mouseClip - Native C++ Win32 Build System
echo ======================================================================
echo.

:: 1. Check if cl.exe is already on PATH
where cl.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [INFO] Found cl.exe in current PATH.
    goto COMPILE
)

:: 2. Locate Visual Studio 2022 / Build Tools via vswhere.exe
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found at "%VSWHERE%".
    echo Please install Visual Studio 2022 or run this script inside Developer Command Prompt.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not defined VS_PATH (
    echo [ERROR] Could not detect a valid Visual Studio installation with C++ tools.
    exit /b 1
)

echo [INFO] Detected Visual Studio: %VS_PATH%
echo [INFO] Initializing MSVC x64 build environment...
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC x64 build environment.
    exit /b 1
)

:COMPILE
if not exist "bin" mkdir "bin"

echo.
echo [1/3] Compiling Application Manifest and Resources...
rc.exe /nologo /fo "src\resource.res" "src\resource.rc"
if errorlevel 1 (
    echo [ERROR] Resource compilation failed.
    exit /b 1
)

echo.
echo [2/3] Compiling Release Background Windowed Binary (mouseClip.exe)...
echo       Flags: /std:c++17 /O2 /MT /SUBSYSTEM:WINDOWS (Hidden console, system tray enabled)
cl.exe /nologo /std:c++17 /O2 /MT /W4 /EHsc ^
    src\main.cpp src\clip_manager.cpp src\target_detector.cpp src\tray_manager.cpp ^
    src\resource.res ^
    /Fe:"bin\mouseClip.exe" ^
    /link /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /MANIFEST:NO ^
    user32.lib kernel32.lib shell32.lib gdi32.lib

if errorlevel 1 (
    echo [ERROR] Release build failed.
    exit /b 1
)

echo.
echo [3/3] Compiling Debug Console Binary (mouseClip_debug.exe)...
echo       Flags: /std:c++17 /Zi /MT /SUBSYSTEM:CONSOLE (Verbose terminal logging enabled)
cl.exe /nologo /std:c++17 /O2 /MT /W4 /EHsc /D "_CONSOLE" ^
    src\main.cpp src\clip_manager.cpp src\target_detector.cpp src\tray_manager.cpp ^
    src\resource.res ^
    /Fe:"bin\mouseClip_debug.exe" ^
    /link /SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF /MANIFEST:NO ^
    user32.lib kernel32.lib shell32.lib gdi32.lib

if errorlevel 1 (
    echo [ERROR] Debug console build failed.
    exit /b 1
)

:: Clean up intermediate object and resource files
del /q *.obj src\resource.res >nul 2>&1

echo.
echo ======================================================================
echo Build Succeeded!
echo Outputs:
echo   [Production] bin\mouseClip.exe       (Background, no console window)
echo   [Diagnostic] bin\mouseClip_debug.exe (Console mode with live logs)
echo ======================================================================
echo.
exit /b 0
