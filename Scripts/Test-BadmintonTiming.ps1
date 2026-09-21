param([string]$GameExecutable,[int]$TimeoutSeconds=180,[switch]$SingleView)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskExe='D:\Game\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$taskPrefix=@('"'+(Join-Path $taskRoot 'FPS_Dell2g.uproject')+'"')
if($GameExecutable){$taskExe=(Resolve-Path -LiteralPath $GameExecutable).Path;$taskPrefix=@()}
$taskLog=Join-Path $taskRoot ('Saved\Logs\BadmintonTiming-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$taskArgs=$taskPrefix+@('/Game/Badminton/Maps/L_Badminton_Prototype','-game','-BadmintonLegacyFirstPerson','-BadmintonTimingTest','-RenderOffscreen','-ForceRes','-windowed','-ResX=1280','-ResY=720','-unattended','-nosound','-nosplash','-ddc=NoZenLocalFallback',"-LocalDataCachePath=$taskRoot\DerivedDataCache",'-ExecCmds="t.MaxFPS 60"',('-abslog="'+$taskLog+'"'))
if($SingleView){$taskArgs+='-BadmintonSingleView'}
$taskProcess=Start-Process -FilePath $taskExe -ArgumentList $taskArgs -PassThru -WindowStyle Hidden
Write-Output "TIMING TEST PID=$($taskProcess.Id) LOG=$taskLog"
try{
 $taskDeadline=(Get-Date).AddSeconds($TimeoutSeconds)
 do{
  $taskText=if(Test-Path -LiteralPath $taskLog){Get-Content -LiteralPath $taskLog -Raw}else{''}
  if($taskText -match 'BADMINTON_TIMING_TEST FAIL|BADMINTON_TIMING_CONTROL_RESTORE_FAILED|Fatal error:|Assertion failed:'){throw "타이밍 검사 실패: $taskLog"}
  if($taskText -match 'BADMINTON_TIMING_TEST PASS[^\r\n]*'){
   $taskPass=$Matches[0]
   if($taskText -match 'BADMINTON_TIMING_CONTROL_RESTORED manualMouse=enabled look=X=0.000 Y=-12.000 timeDilation=1' -and $taskText -match 'BADMINTON_MINIMAP_TEST PASS players=2 shuttle=1 landing=1|BADMINTON_DUAL_VIEW_UI_TEST PASS shuttle=1 landing=1' -and $taskText -match 'BADMINTON_WHEEL_INPUT_TEST PASS' -and $taskText -match 'selectionReset=OK directionPreview=OK' -and $taskText -match 'BADMINTON_DIRECTION_UI_TEST PASS' -and $taskText -match 'BADMINTON_RACKET_IMPACT shot=2 angle=3.0 deltaVelocity=([1-9][0-9]*\.[0-9]+)'){
    Write-Output $taskPass
    Write-Output 'Manual mouse, forward camera and normal speed restored'
    return
   }
  }
  if($taskProcess.HasExited){throw "게임 조기 종료: $taskLog"}
  Start-Sleep -Milliseconds 500
 }while((Get-Date)-lt $taskDeadline)
 throw "검사 시간 초과: $taskLog"
}finally{if(-not $taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}}
