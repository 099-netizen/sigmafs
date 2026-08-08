@echo off
REM ============================================================================
REM  build.bat  — One-click EXE builder for 999 SERVICES FiveM Offset Dumper
REM  Run this from a "x64 Native Tools Command Prompt for VS 2022" OR just
REM  double-click it — it will auto-locate MSVC via vswhere.
REM ============================================================================
setlocal EnableDelayedExpansion
chcp 65001 >nul
pushd "%~dp0"

echo.
echo  [999 SERVICES]  FiveM Offset Dumper - Build Script
echo ====================================================

REM ---------- Step 1: Make sure lib/imgui is populated ----------
if not exist lib\imgui\imgui.cpp (
    echo [!] Dear ImGui not found. Running setup.bat ...
    call setup.bat
    if errorlevel 1 (
        echo [X] Setup failed!
        pause & exit /b 1
    )
)

REM ---------- Step 2: Locate MSVC (VS 2022 / Build Tools) ----------
set "VCVARS="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
)

if not exist "%VCVARS%" (
    echo [X] Could not find Visual Studio 2022 (or Build Tools) with C++ workload.
    echo     Install it from: https://visualstudio.microsoft.com/downloads/
    pause & exit /b 1
)

echo [+] Loading MSVC x64 environment...
call "%VCVARS%" >nul 2>&1

REM ---------- Step 3: Configure + build using CMake ----------
if not exist build mkdir build
pushd build

echo [+] Configuring (CMake)...
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release >cmake.log 2>&1
if errorlevel 1 (
    echo [!] Ninja not found, falling back to NMake...
    cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release >cmake.log 2>&1
    if errorlevel 1 goto :fail
)

echo [+] Compiling (this may take ~30 sec)...
cmake --build . --config Release >>cmake.log 2>&1
if errorlevel 1 goto :fail

popd

REM ---------- Step 4: Show output ----------
set "EXE=build\bin\999_Offset_Dumper.exe"
if not exist "%EXE%" set "EXE=build\999_Offset_Dumper.exe"
if not exist "%EXE%" set "EXE=build\Release\999_Offset_Dumper.exe"

if exist "%EXE%" (
    echo.
    echo ====================================================
    echo  [OK] Build SUCCESS!
    echo  EXE: %CD%\%EXE%
    echo ====================================================
    echo.
    explorer /select,"%CD%\%EXE%"
    goto :end
)

:fail
popd
echo.
echo [X] Build FAILED. See build\cmake.log for errors.
if exist build\cmake.log type build\cmake.log
exit /b 1

:end
popd
endlocal
pause
