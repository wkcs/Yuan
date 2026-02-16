@echo off
setlocal enabledelayedexpansion

set ROOT_DIR=%~dp0\..
for %%I in ("%ROOT_DIR%") do set ROOT_DIR=%%~fI
set BUILD_DIR=%ROOT_DIR%\build
set OUT_DLL=%BUILD_DIR%\gui\yuan_gui_windows.dll

cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%"
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --target yuan_gui_windows
if errorlevel 1 exit /b 1

echo Built: %OUT_DLL%
