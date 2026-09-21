@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" pico2w
set "prince_build_exit=%errorlevel%"
if not "%prince_build_exit%"=="0" pause
exit /b %prince_build_exit%
