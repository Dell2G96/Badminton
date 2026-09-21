param(
    [string]$EngineRoot = 'D:\Game\UE_5.7',
    [string]$GameExecutable,
    [ValidateRange(30,600)][int]$TimeoutSeconds = 240,
    [switch]$Render
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskExecutable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$taskPrefix = @('"' + (Join-Path $taskRoot 'FPS_Dell2g.uproject') + '"')
if ($GameExecutable) {
    $taskExecutable = (Resolve-Path -LiteralPath $GameExecutable).Path
    $taskPrefix = @()
}
$taskRun = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$taskLog = Join-Path $taskRoot "Saved\Logs\BadmintonAI-$taskRun.log"
$taskArgs = $taskPrefix + @('/Game/Badminton/Maps/L_Badminton_Prototype', '-game', '-BadmintonAITest', '-BadmintonLandingTest', '-unattended', '-nosplash',
    '-ddc=NoZenLocalFallback', "-LocalDataCachePath=$taskRoot\DerivedDataCache", '-ExecCmds="t.MaxFPS 60"', ('-abslog="' + $taskLog + '"'))
if ($Render) { $taskArgs += @('-windowed', '-ResX=1280', '-ResY=720', '-BadmintonCapture', '-BadmintonCaptureOnContact') }
else { $taskArgs += @('-nullrhi', '-nosound') }
$taskProcess = $null
try {
    $taskProcess = Start-Process -FilePath $taskExecutable -ArgumentList $taskArgs -PassThru -WindowStyle Hidden
    Write-Output "AI 검증 시작: PID=$($taskProcess.Id), 로그=$taskLog"
    $taskDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        $taskText = if (Test-Path -LiteralPath $taskLog) { Get-Content -LiteralPath $taskLog -Raw } else { '' }
        if ($taskText -match 'BADMINTON_AI_TEST FAIL|BADMINTON_LANDING_CHECK FAIL|Fatal error:|Assertion failed:') { throw "AI 검증 실패: $taskLog" }
        if ($taskText -match 'BADMINTON_AI_TEST PASS[^\r\n]*') {
            $taskResult = $Matches[0]
            $taskPoints = [regex]::Matches($taskText, 'BADMINTON_POINT rally=(\d+) score=(\d+):(\d+)')
            if ($taskPoints.Count -lt 11 -or @($taskPoints | ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique).Count -ne $taskPoints.Count) {
                throw '득점 누락 또는 중복 랠리 득점'
            }
            if ([regex]::Matches($taskText, 'BADMINTON_AI_SPAWNED').Count -ne 1) { throw 'AI 생성 수 불일치' }
            if ($taskText -notmatch 'BADMINTON_SHOT_ACCEPTED side=1 .*type=0') { throw 'AI 서브 누락' }
            if ($taskText -notmatch 'BADMINTON_SHOT_ACCEPTED side=1 .*type=[23]') { throw 'AI 샷 선택 누락' }
            if ([regex]::Matches($taskText, 'BADMINTON_LANDING_CHECK PASS').Count -lt 5) { throw '실제 착지 비교 표본 부족' }
            Write-Output $taskResult
            Write-Output '실제 착지와 표시 위치 오차 2cm 미만 검증 통과'
            Write-Output "AI 서브, 이동, 샷 선택, 랠리, 중복 득점 방지, 11점 종료 및 재경기 검증 통과: $taskLog"
            return
        }
        if ($taskProcess.HasExited) { throw "게임 조기 종료: $taskLog" }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $taskDeadline)
    throw "AI 검증 시간 초과: $taskLog"
} finally {
    if ($taskProcess -and -not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id -Force }
}
