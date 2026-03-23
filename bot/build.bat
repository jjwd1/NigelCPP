@echo off
title Nigel SDK Builder
echo.
echo  Nigel SDK - Build from source
echo  ==============================
echo.

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

cd /d "%~dp0"

echo === CMake Configure ===
cmake -B out\build\x64-Release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
if %ERRORLEVEL% NEQ 0 (
    echo CMake configure FAILED
    pause
    exit /b 1
)

echo === Building ===
cmake --build out\build\x64-Release
if %ERRORLEVEL% NEQ 0 (
    echo Build FAILED
    pause
    exit /b 1
)

echo === Copying DLL ===
copy /Y "out\build\x64-Release\NigelSDK.dll" "%~dp0NigelSDK.dll"

echo.
echo === Build SUCCESS ===
echo.
pause
