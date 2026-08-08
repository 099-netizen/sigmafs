@echo off
REM ---------------------------------------------------------------
REM  run.bat  -  Runs the EXE and keeps window open even if it crashes
REM ---------------------------------------------------------------
@chcp 65001 >nul
pushd "%~dp0"

set "EXE="
if exist build\bin\Release\999_Offset_Dumper.exe      set "EXE=build\bin\Release\999_Offset_Dumper.exe"
if exist build\bin\999_Offset_Dumper.exe              set "EXE=build\bin\999_Offset_Dumper.exe"
if exist build\Release\999_Offset_Dumper.exe          set "EXE=build\Release\999_Offset_Dumper.exe"
if exist bin\Release\999_Offset_Dumper.exe            set "EXE=bin\Release\999_Offset_Dumper.exe"
if exist vs\bin\Release\999_Offset_Dumper.exe         set "EXE=vs\bin\Release\999_Offset_Dumper.exe"
if exist 999_Offset_Dumper.exe                        set "EXE=999_Offset_Dumper.exe"

if not defined EXE (
    color 0C
    echo.
    echo  [X] EXE eka hoyagatta nah!
    echo      Mulin "build_and_run.bat" run karala build karanna.
    echo.
    pause
    exit /b 1
)

echo.
echo  ====================================================
echo   Launching: %EXE%
echo   (Window eka crash unath cmd eka nawathinawa)
echo  ====================================================
echo.

"%EXE%"
set "RC=%ERRORLEVEL%"

echo.
echo  ----------------------------------------------------
echo  Program eka close una. Exit code: %RC%
echo  (0 = normal exit, -107374... = crash/access violation)
echo  ----------------------------------------------------
echo.
pause
popd
