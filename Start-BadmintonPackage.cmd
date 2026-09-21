@echo off
chcp 65001 >nul
setlocal
set "TaskPowerShell=C:\Users\User\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe"
if not exist "%TaskPowerShell%" (
  echo PowerShell 7을 찾을 수 없습니다. PowerShell 7에서 Scripts\Start-BadmintonPackage.ps1을 실행하세요.
  pause
  exit /b 1
)
echo 1. 컴퓨터 상대 경기 (오프라인)
echo 2. 로컬 IP 경기방 만들기
echo 3. 로컬 호스트 참가 (127.0.0.1:7777)
echo 4. 인터넷 경기방 (EOS)
choice /c 1234 /n /m "실행 모드를 선택하세요: "
set "TaskMode=Solo"
if errorlevel 2 set "TaskMode=Host"
if errorlevel 3 set "TaskMode=Join"
if errorlevel 4 set "TaskMode=EOS"
"%TaskPowerShell%" -NoProfile -File "%~dp0Scripts\Start-BadmintonPackage.ps1" -Mode %TaskMode%
if errorlevel 1 pause
