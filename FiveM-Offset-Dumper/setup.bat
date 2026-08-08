@echo off
REM ============================================================================
REM  setup.bat  -  Downloads Dear ImGui into lib/imgui
REM ============================================================================
@echo off
chcp 65001 >nul
setlocal
pushd "%~dp0"

echo.
echo  [999/H89] Setting up Dear ImGui...
echo.

if not exist lib mkdir lib
if not exist lib\imgui mkdir lib\imgui

if exist lib\imgui\imgui.cpp (
    echo [OK] ImGui already present.
    goto :done
)

if not exist lib\imgui-master (
    echo [*] Downloading ImGui from github.com/ocornut/imgui ...
    powershell -NoProfile -Command "$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri 'https://github.com/ocornut/imgui/archive/refs/heads/master.zip' -OutFile 'lib\\imgui.zip'"
    if errorlevel 1 (
        echo [X] Download failed. Check internet / run as admin.
        pause & exit /b 1
    )
    echo [*] Extracting...
    powershell -NoProfile -Command "Expand-Archive -Path 'lib\\imgui.zip' -DestinationPath 'lib' -Force"
    del /q lib\imgui.zip >nul 2>&1
)

echo [*] Copying ImGui core + backends...
copy /Y lib\imgui-master\imgui*.h            lib\imgui\ >nul
copy /Y lib\imgui-master\imgui*.cpp          lib\imgui\ >nul
copy /Y lib\imgui-master\imstb*.h            lib\imgui\ >nul
copy /Y lib\imgui-master\imconfig.h          lib\imgui\ >nul
copy /Y lib\imgui-master\backends\imgui_impl_win32.*  lib\imgui\ >nul
copy /Y lib\imgui-master\backends\imgui_impl_dx11.*   lib\imgui\ >nul

:done
echo [OK] Setup complete.
echo.
popd
endlocal
