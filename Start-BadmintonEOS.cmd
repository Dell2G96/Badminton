@echo off
chcp 65001 >nul
setlocal
set "EOS_GAME_RUNTIME=C:\Users\User\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe"
if not exist "%EOS_GAME_RUNTIME%" (
  echo PowerShell을 찾을 수 없습니다. 실행 메뉴의 설치 경로를 확인하세요.
  pause
  exit /b 1
)
"%EOS_GAME_RUNTIME%" -NoProfile -File "%~dp0Scripts\Start-BadmintonPackage.ps1" -Mode EOS -PackageName BadmintonForgivingTiming
if errorlevel 1 pause
endlocal
