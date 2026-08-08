@echo off
REM ============================================================================
REM  build_and_run.bat - Builds AND runs the dumper, keeps window open on error
REM ============================================================================
setlocal EnableDelayedExpansion
chcp 65001 >nul
pushd "%~dp0"

color 0B
echo.
echo  ====================================================
echo   999 SERVICES - FiveM Offset Dumper (Build ^& Run)
echo  ====================================================
echo.

REM ---- Check ImGui ----
if not exist lib\imgui\imgui.cpp (
    echo [*] Downloading Dear ImGui...
    call setup.bat
    echo.
)

if not exist lib\imgui\imgui.cpp (
    color 0C
    echo [X] IMGUI NOT FOUND! Please run setup.bat first!
    pause
    exit /b 1
)

REM ---- Check for MSVC ----
set "VCVARS="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
    set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
)

if not exist "%VCVARS%" (
    color 0C
    echo [X] Visual Studio 2022 C++ NOT FOUND!
    echo     Install VS2022 Community (free) with "Desktop C++" workload.
    echo     https://visualstudio.microsoft.com/downloads/
    echo.
    pause
    exit /b 1
)

echo [+] Setting up MSVC x64 compiler...
call "%VCVARS%" >nul 2>&1

REM ---- Build folder ----
if not exist build mkdir build
cd build

echo [+] Running CMake configure...
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    color 0C
    echo.
    echo [X] CMake configuration FAILED! (Look at errors above)
    echo.
    pause
    exit /b 1
)

echo [+] Compiling...
cmake --build . --config Release
if errorlevel 1 (
    color 0C
    echo.
    echo [X] COMPILATION FAILED!
    echo     Scroll up and read the first error (usually a missing header or typo).
    echo.
    pause
    exit /b 1
)

cd ..

REM ---- Find EXE ----
set "EXE="
if exist build\bin\999_Offset_Dumper.exe         set "EXE=build\bin\999_Offset_Dumper.exe"
if exist build\999_Offset_Dumper.exe             set "EXE=build\999_Offset_Dumper.exe"
if exist build\Release\999_Offset_Dumper.exe     set "EXE=build\Release\999_Offset_Dumper.exe"

if not defined EXE (
    color 0C
    echo [X] Build finished but exe not found! Check the build folder manually.
    pause
    exit /b 1
)

echo.
echo [OK] Build successful!
echo [*] EXE: %CD%\%EXE%
echo.
echo ====================================================
echo [*] Running the program... (Admin recommended)
echo ====================================================
echo.

REM Run the EXE and wait for it to finish - don't close window automatically
"%EXE%"
set EXITCODE=%ERRORLEVEL%

echo.
echo ====================================================
echo Program exited with code %EXITCODE%
echo Window will stay open so you can read the output.
echo ====================================================
echo.
pause
endlocal
