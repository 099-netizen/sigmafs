@echo off
REM ================================================================
REM  build_simple.bat  -  NO CMake, NO auto-close
REM  Compiles with plain cl.exe (MSVC). Pauses on every error.
REM  Just double-click this file.
REM ================================================================
@chcp 65001 >nul
@setlocal
@pushd "%~dp0"
@color 0A

echo.
echo  ############################################################
echo   999 SERVICES - FiveM Offset Dumper  (SIMPLE BUILD)
echo  ############################################################
echo.

REM ---- Locate MSVC ----
@set "VCVARS="
@if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"   set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
@if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
@if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"   set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
@if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"   set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
@if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
@if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do @set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
)

@if not defined VCVARS goto :nomsvc
@if not exist "%VCVARS%" goto :nomsvc
echo [*] Loading MSVC: "%VCVARS%"
@call "%VCVARS%" >nul 2>&1

@where cl.exe >nul 2>&1
@if errorlevel 1 goto :nomsvc
echo [OK] cl.exe ready.

REM ---- ImGui check ----
@if not exist lib\imgui\imgui.cpp (
    echo [!] Downloading ImGui...
    @call setup.bat
)
@if not exist lib\imgui\imgui.cpp goto :noimgui
echo [OK] ImGui found.

REM ---- Create output dirs ----
@if not exist obj\imgui @mkdir obj\imgui
@if not exist bin\Release @mkdir bin\Release

REM ---- Compile ImGui core ----
echo [*] Compiling ImGui...
@set CXX=/nologo /std:c++17 /EHsc /O2 /MD /W3 /DUNICODE /D_UNICODE /DNDEBUG
@set INC=/I src /I lib /I lib\imgui
for %%o in (imgui imgui_demo imgui_draw imgui_tables imgui_widgets imgui_impl_win32 imgui_impl_dx11) do (
    echo     - %%o.cpp
    @cl %CXX% %INC% /c lib\imgui\%%o.cpp /Fo:obj\imgui\%%o.obj >obj\imgui\%%o.log 2>&1
    @if errorlevel 1 goto :fail
)

echo [*] Compiling main.cpp...
@cl %CXX% %INC% /c src\main.cpp /Fo:obj\main.obj >obj\main.log 2>&1
@if errorlevel 1 (type obj\main.log & goto :fail)

echo [*] Linking...
@cl obj\imgui\*.obj obj\main.obj /nologo /Fe:bin\Release\999_Offset_Dumper.exe ^
   /link /SUBSYSTEM:WINDOWS d3d11.lib dxgi.lib d3dcompiler.lib psapi.lib /MACHINE:X64 >obj\link.log 2>&1
@if errorlevel 1 (type obj\link.log & goto :fail)

echo.
echo  ############################################################
echo   [OK] BUILD SUCCESS!
echo   EXE: bin\Release\999_Offset_Dumper.exe
echo  ############################################################
echo.
echo [*] Launching EXE in 3 seconds... (close the window to come back)
@ping -n 4 127.0.0.1 >nul
start "" /wait "bin\Release\999_Offset_Dumper.exe"
echo [i] Program closed. Exit code: %ERRORLEVEL%
goto :end

:nomsvc
echo.
echo  [X] Visual Studio C++ compiler not found!
echo      Install "Visual Studio 2022 Community" (FREE):
echo      https://visualstudio.microsoft.com/downloads/
echo      During install tick "Desktop development with C++".
goto :end

:noimgui
echo.
echo  [X] lib\imgui\imgui.cpp not found. Double-check setup.bat finished.
goto :end

:fail
echo.
echo  [X] BUILD FAILED! See error log(s) in obj\ folder.
goto :end

:end
echo.
echo  ============================================================
echo   Press any key to close this window...
echo  ============================================================
pause >nul
@endlocal
