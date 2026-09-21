@echo off
chcp 65001 >nul
setlocal
set "TaskPowerShell=C:\Users\User\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe"
if not exist "%TaskPowerShell%" (
  echo PowerShell 7을 찾을 수 없습니다. PowerShell 7에서 Scripts\Start-BadmintonPackage.ps1을 실행하세요.
  pause
  exit /b 1
)
"%TaskPowerShell%" -NoProfile -File "%~dp0Scripts\Start-BadmintonPackage.ps1" -Mode Solo -PackageName BadmintonForgivingTiming
if errorlevel 1 pause
