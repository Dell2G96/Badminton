param([Parameter(Mandatory=$true)][string]$GameExecutable)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskLog=Join-Path $taskRoot ('Saved\Logs\RacketPose-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$taskArgs=@('/Game/Badminton/Maps/L_Badminton_Prototype','-BadmintonRacketPoseTest','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-unattended','-nosound','-ExecCmds="t.MaxFPS 60"',('-abslog="'+$taskLog+'"'))
$taskProcess=Start-Process -FilePath (Resolve-Path -LiteralPath $GameExecutable).Path -ArgumentList $taskArgs -PassThru -WindowStyle Hidden
Write-Output "RACKET POSE PID=$($taskProcess.Id) LOG=$taskLog"
try {
 $taskDeadline=(Get-Date).AddSeconds(60)
 do {
  $taskText=if(Test-Path -LiteralPath $taskLog){Get-Content -LiteralPath $taskLog -Raw}else{''}
  if($taskText -match 'BADMINTON_TIMING_TEST FAIL|RESTORE_FAILED|Fatal error:|Assertion failed:'){throw "라켓 자세 검사 실패: $taskLog"}
  if($taskText -match 'BADMINTON_RACKET_POSE_TEST PASS' -and $taskText -match 'BADMINTON_TIMING_CONTROL_RESTORED'){
   Write-Output 'Left / overhead / right screenshots saved; hit ray matches racket; manual control restored'
   return
  }
  if($taskProcess.HasExited){throw "게임 조기 종료: $taskLog"}
  Start-Sleep -Milliseconds 500
 } while((Get-Date) -lt $taskDeadline)
 throw "라켓 자세 검사 시간 초과: $taskLog"
} finally {if(-not $taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}}
