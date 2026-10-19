@echo off
cd /d "%~dp0"
if not exist "FIELD.exe" powershell.exe -NoProfile -File "%~dp0bridge\build.ps1"
if not exist "FIELD.exe" goto failed
"%~dp0FIELD.exe"
if errorlevel 1 goto failed
exit /b 0
:failed
echo FIELD could not start. Check the error above.
pause
