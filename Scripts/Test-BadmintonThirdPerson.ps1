param([string]$GameExecutable, [int]$Width=1280, [int]$Height=720)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskExe='D:\Game\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$taskPrefix=@('"'+(Join-Path $taskRoot 'FPS_Dell2g.uproject')+'"')
if($GameExecutable){$taskExe=(Resolve-Path -LiteralPath $GameExecutable).Path;$taskPrefix=@()}
$taskLog=Join-Path $taskRoot ('Saved\Logs\BadmintonThirdPerson-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$taskArgs=$taskPrefix+@('/Game/Badminton/Maps/L_Badminton_Prototype','-game','-BadmintonThirdPersonTest','-RenderOffscreen','-ForceRes','-windowed',"-ResX=$Width","-ResY=$Height",'-unattended','-nosound','-nosplash','-ddc=NoZenLocalFallback',"-LocalDataCachePath=$taskRoot\DerivedDataCache",'-ExecCmds="t.MaxFPS 60"',('-abslog="'+$taskLog+'"'))
$taskProcess=Start-Process -FilePath $taskExe -ArgumentList $taskArgs -PassThru -WindowStyle Hidden
Write-Output "THIRD PERSON TEST PID=$($taskProcess.Id) LOG=$taskLog"
try {
    $taskDeadline=(Get-Date).AddSeconds(160)
    do {
        $taskText=if(Test-Path -LiteralPath $taskLog){Get-Content -LiteralPath $taskLog -Raw}else{''}
        if($taskText -match 'BADMINTON_THIRD_PERSON_TEST FAIL|Fatal error:|Assertion failed:'){throw "3인칭 타이밍 검사 실패: $taskLog"}
        if($taskText -match 'BADMINTON_THIRD_PERSON_TEST PASS[^\r\n]*'){Write-Output $Matches[0]; return}
        if($taskProcess.HasExited){throw "게임 조기 종료: $taskLog"}
        Start-Sleep -Milliseconds 500
    }while((Get-Date) -lt $taskDeadline)
    throw "3인칭 타이밍 검사 시간 초과: $taskLog"
}finally{if(-not $taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}}
