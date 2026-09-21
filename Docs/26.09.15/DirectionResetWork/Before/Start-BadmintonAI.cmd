@echo off
setlocal
set "TaskPowerShell=C:\Users\User\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe"
if not exist "%TaskPowerShell%" (
  echo PowerShell 7 runtime not found. Run Scripts\Start-BadmintonPackage.ps1 -Mode Solo -PackageName BadmintonFPSTransparentRacket with PowerShell 7.
  pause
  exit /b 1
)
"%TaskPowerShell%" -NoProfile -File "%~dp0Scripts\Start-BadmintonPackage.ps1" -Mode Solo -PackageName BadmintonFPSTransparentRacket
if errorlevel 1 pause
