param([Parameter(Mandatory=$true)][string]$GameExecutable)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskLog=Join-Path $taskRoot ('Saved\Logs\CameraHandoffFailure-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$taskArgs=@('/Game/Badminton/Maps/L_Badminton_Prototype','-game','-BadmintonTimingTest','-BadmintonTimingAbortAfter=1','-nullrhi','-unattended','-nosound','-nosplash','-ExecCmds="t.MaxFPS 60,slomo 0.4"',('-abslog="'+$taskLog+'"'))
$taskProcess=Start-Process -FilePath $GameExecutable -ArgumentList $taskArgs -PassThru -WindowStyle Hidden
Write-Output "HANDOFF FAILURE CHECK PID=$($taskProcess.Id) LOG=$taskLog"
try {
 $taskDeadline=(Get-Date).AddSeconds(45)
 do {
  $taskText=if(Test-Path -LiteralPath $taskLog){Get-Content -LiteralPath $taskLog -Raw}else{''}
  if($taskText -match 'BADMINTON_TIMING_CONTROL_RESTORE_FAILED|Fatal error:|Assertion failed:'){throw "Control restoration failed: $taskLog"}
  if($taskText -match 'BADMINTON_TIMING_TEST FAIL requested abort for control restoration check' -and $taskText -match 'BADMINTON_TIMING_CONTROL_RESTORED manualMouse=enabled look=X=0.000 Y=-12.000 timeDilation=1'){
   Write-Output 'PASS: controlled failure returns mouse, forward camera and normal speed'
   return
  }
  if($taskProcess.HasExited){throw "Game exited before handoff verification: $taskLog"}
  Start-Sleep -Milliseconds 250
 }while((Get-Date) -lt $taskDeadline)
 throw "Control restoration check timed out: $taskLog"
}finally{if(-not $taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}}
