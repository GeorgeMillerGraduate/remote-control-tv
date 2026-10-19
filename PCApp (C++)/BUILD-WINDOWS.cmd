@echo off
setlocal
cd /d "%~dp0"
echo Jenga Remote - Automatic Windows build
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-windows.ps1" -Launch -PauseOnError
if errorlevel 1 exit /b 1
