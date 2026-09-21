param([string]$EngineRoot='D:\Game\UE_5.7', [int]$TimeoutSeconds=180)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskLog=Join-Path $taskRoot ('Saved\Logs\BadmintonDualViewPIE-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$taskArgs=@(('"'+(Join-Path $taskRoot 'FPS_Dell2g.uproject')+'"'),'-RenderOffscreen','-unattended','-nop4','-nosound','-nosplash','-ddc=NoZenLocalFallback',('-LocalDataCachePath="'+$taskRoot+'\DerivedDataCache"'),'-ExecCmds="Automation RunTests FPS_Dell2g.Badminton.Camera.PlayModePIE"','-TestExit="Automation Test Queue Empty"',('-abslog="'+$taskLog+'"'))
$taskProcess=Start-Process -FilePath (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') -ArgumentList $taskArgs -PassThru -WindowStyle Hidden
Write-Output "PIE TEST PID=$($taskProcess.Id) LOG=$taskLog"
try {
    $taskDeadline=(Get-Date).AddSeconds($TimeoutSeconds)
    while(-not $taskProcess.HasExited) {
        if((Get-Date) -gt $taskDeadline){throw "PIE 검사 시간 초과: $taskLog"}
        Start-Sleep -Milliseconds 500
    }
    $taskText=Get-Content -LiteralPath $taskLog -Raw
    if($taskProcess.ExitCode -ne 0 -or $taskText -notmatch 'Result=\{Success\}[^\r\n]*FPS_Dell2g.Badminton.Camera.PlayModePIE') {
        throw "PIE 검사 실패: $taskLog"
    }
    Write-Output 'PASS: actual PIE, active camera mode, viewport layout, toggle behavior and aim projection'
}finally{if(-not $taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}}
