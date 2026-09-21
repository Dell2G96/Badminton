@echo off
setlocal
set "EOS_SETUP_RUNTIME=C:\Users\User\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe"
if not exist "%EOS_SETUP_RUNTIME%" (
  echo PowerShell runtime not found. Ask Codex to update this launcher.
  pause
  exit /b 1
)
"%EOS_SETUP_RUNTIME%" -NoProfile -STA -File "%~dp0Scripts\Set-BadmintonEOSCredentials.ps1"
if errorlevel 1 pause
endlocal
