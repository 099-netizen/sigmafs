@echo off
chcp 65001 >nul
setlocal
pushd "%~dp0"
color 0B

echo.
echo  H89 DEV - Rebuild (clean ^& compile)
echo  ===================================
echo.

REM ---- 1. Make sure ImGui is present ----
if not exist lib\imgui\imgui.cpp (
    echo [!] ImGui missing, running setup...
    call setup.bat
    echo.
)
if not exist lib\imgui\imgui.cpp (
    color 0C
    echo [X] Still cannot find lib\imgui\imgui.cpp - run setup.bat manually.
    pause & exit /b 1
)
echo [OK] ImGui ready.

REM ---- 2. Locate MSVC ----
set "VCVARS="
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"     set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"  set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"    set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"    set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS (
    color 0C
    echo [X] MSVC not found. Install VS 2022 Build Tools/Community with C++ workload.
    pause & exit /b 1
)
call "%VCVARS%" >nul 2>&1
where cl.exe >nul 2>&1
if errorlevel 1 (
    color 0C
    echo [X] cl.exe not available after vcvars64.
    pause & exit /b 1
)
echo [OK] MSVC loaded.

REM ---- 3. Clean output ----
if exist obj rmdir /s /q obj
if exist bin rmdir /s /q bin
mkdir obj\imgui 2>nul
mkdir bin\Release 2>nul

REM ---- 4. Compiler flags ----
set CXX=/nologo /std:c++17 /EHsc /O2 /MD /W3 /DUNICODE /D_UNICODE /DNDEBUG
set INC=/I "%cd%\src" /I "%cd%\lib" /I "%cd%\lib\imgui"
set LNK=/link /OUT:"%cd%\bin\Release\H89_Dumper.exe" /SUBSYSTEM:WINDOWS d3d11.lib dxgi.lib d3dcompiler.lib shell32.lib psapi.lib /MACHINE:X64

echo [*] Compiling ImGui core...
for %%o in (imgui imgui_demo imgui_draw imgui_tables imgui_widgets imgui_impl_win32 imgui_impl_dx11) do (
    cl %CXX% %INC% /c "lib\imgui\%%o.cpp" /Fo:obj\imgui\%%o.obj >nul 2>obj\imgui\%%o.log
    if errorlevel 1 (
        color 0C
        echo [X] Failed: %%o.cpp
        type obj\imgui\%%o.log
        pause & exit /b 1
    )
)

echo [*] Compiling src\main.cpp...
cl %CXX% %INC% /c src\main.cpp /Fo:obj\main.obj 2>&1
if errorlevel 1 (
    color 0C
    echo.
    echo [X] main.cpp compile failed (see above).
    pause & exit /b 1
)

echo [*] Linking...
cl obj\imgui\*.obj obj\main.obj %LNK% >obj\link.log 2>&1
if errorlevel 1 (
    color 0C
    type obj\link.log
    echo [X] Link failed.
    pause & exit /b 1
)

echo.
echo  ====================================================
echo   [OK] BUILD SUCCESS
echo   EXE: %cd%\bin\Release\H89_Dumper.exe
echo  ====================================================
echo.
echo Starting program...
start "" "%cd%\bin\Release\H89_Dumper.exe"

popd
endlocal
