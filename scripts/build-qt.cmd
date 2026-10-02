@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
setlocal EnableExtensions DisableDelayedExpansion
rem Native Qt / C++ Windows x64 build, tests and portable distribution.
if not "%~1"=="" if /i not "%~1"=="--rebuild" (
    echo Usage: build-release.cmd [--rebuild]
    exit /b 2
)
if not "%~2"=="" (
    echo Usage: build-release.cmd [--rebuild]
    exit /b 2
)
for %%I in ("%~dp0..") do set "REPO=%%~fI"
if not defined QT_DIR set "QT_DIR=C:\Qt\Qt5.11.1\5.11.1\msvc2017_64"
if not defined VCVARS64 set "VCVARS64=C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not defined VC_REDIST_DIR set "VC_REDIST_DIR=C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Redist\MSVC\14.16.27012\x64\Microsoft.VC141.CRT"
set "BUILD=%REPO%\artifacts\qt-release"
set "STAGE=%REPO%\dist\staging-%RANDOM%-%RANDOM%"
set "DIST=%STAGE%\Release"
for %%G in (git.exe) do set "GIT_EXE=%%~$PATH:G"
set "POWERSHELL=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%QT_DIR%\bin\qmake.exe" (echo Qt qmake not found. Set QT_DIR. & exit /b 1)
if not exist "%QT_DIR%\bin\windeployqt.exe" (echo Qt deployment tool not found. Set QT_DIR. & exit /b 1)
if not exist "%VCVARS64%" (echo MSVC environment not found. Set VCVARS64. & exit /b 1)
if not exist "%VC_REDIST_DIR%\vcruntime140.dll" (echo MSVC runtime not found. Set VC_REDIST_DIR. & exit /b 1)
call "%VCVARS64%"
if errorlevel 1 exit /b 1
set "PATH=%QT_DIR%\bin;%PATH%"
set "QT_PLUGIN_PATH="
set "QT_QPA_PLATFORM_PLUGIN_PATH="
if /i "%~1"=="--rebuild" (
    "%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%REPO%\scripts\clean-qt.ps1" -Target Build
    if errorlevel 1 exit /b 1
)
"%QT_DIR%\bin\qmake.exe" -v
if not exist "%BUILD%" mkdir "%BUILD%"
"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%REPO%\scripts\snapshot-sources.ps1" -Output "%BUILD%\source-inputs.json"
if errorlevel 1 exit /b 1
call :build native "%REPO%\BallisticCalculator.pro"
if errorlevel 1 exit /b 1
pushd "%BUILD%\native\tests\core"
release\core_tests.exe -o core-tests.txt,txt
set "TEST_RESULT=%ERRORLEVEL%"
type core-tests.txt
popd
if not "%TEST_RESULT%"=="0" exit /b %TEST_RESULT%
pushd "%BUILD%\native\tests\ui"
release\ui_tests.exe -platform windows -o ui-tests.txt,txt
set "TEST_RESULT=%ERRORLEVEL%"
type ui-tests.txt
popd
if not "%TEST_RESULT%"=="0" exit /b %TEST_RESULT%
"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%REPO%\tests\scripts\release_tests.ps1"
if errorlevel 1 exit /b 1
mkdir "%DIST%"
if errorlevel 1 exit /b 1
copy /y "%BUILD%\native\src\app\release\BallisticCalculator.exe" "%DIST%\" >nul
if errorlevel 1 exit /b 1
"%QT_DIR%\bin\windeployqt.exe" --release --no-translations --no-compiler-runtime "%DIST%\BallisticCalculator.exe"
if errorlevel 1 exit /b 1
copy /y "%VC_REDIST_DIR%\*.dll" "%DIST%\" >nul
if errorlevel 1 exit /b 1
copy /y "%REPO%\src\app\qt.conf" "%DIST%\qt.conf" >nul
if errorlevel 1 exit /b 1
copy /y "%REPO%\LICENSE" "%DIST%\LICENSE" >nul
if errorlevel 1 exit /b 1
copy /y "%REPO%\NOTICE" "%DIST%\NOTICE" >nul
if errorlevel 1 exit /b 1
copy /y "%REPO%\README.md" "%DIST%\README.md" >nul
if errorlevel 1 exit /b 1
if not exist "%DIST%\docs" mkdir "%DIST%\docs"
copy /y "%REPO%\docs\*.md" "%DIST%\docs\" >nul
if errorlevel 1 exit /b 1
"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%REPO%\scripts\qt-license-notices.ps1" -QtDir "%QT_DIR%" -Destination "%DIST%\licenses"
if errorlevel 1 exit /b 1
copy /y "%REPO%\LICENSES\MIT-legacy.txt" "%DIST%\licenses\MIT-legacy.txt" >nul
if errorlevel 1 exit /b 1
rem qt.conf locates the packaged plugins. No SDK paths are needed at runtime.
set "PATH=%SystemRoot%\system32;%SystemRoot%"
set "QT_PLUGIN_PATH="
set "QT_QPA_PLATFORM_PLUGIN_PATH="
pushd "%DIST%"
BallisticCalculator.exe -platform windows --smoke-test "%BUILD%\screenshots"
set "TEST_RESULT=%ERRORLEVEL%"
popd
if not "%TEST_RESULT%"=="0" exit /b %TEST_RESULT%
"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%REPO%\scripts\package-qt.ps1" -DistributionDirectory "%DIST%" -QtDirectory "%QT_DIR%" -GitExecutable "%GIT_EXE%" -ExpectedSourceManifest "%BUILD%\source-inputs.json"
if errorlevel 1 exit /b 1
"%POWERSHELL%" -NoProfile -ExecutionPolicy Bypass -File "%REPO%\scripts\publish-qt.ps1" -StagingDirectory "%STAGE%"
if errorlevel 1 exit /b 1
echo Ready: %REPO%\dist\Release\BallisticCalculator.exe
exit /b 0

:build
if not exist "%BUILD%\%~1" mkdir "%BUILD%\%~1"
if errorlevel 1 exit /b 1
pushd "%BUILD%\%~1"
if errorlevel 1 exit /b 1
"%QT_DIR%\bin\qmake.exe" -r "%~2" CONFIG+=release CONFIG-=debug
if errorlevel 1 (popd & exit /b 1)
nmake /nologo /s
set "BUILD_RESULT=%ERRORLEVEL%"
popd
exit /b %BUILD_RESULT%
