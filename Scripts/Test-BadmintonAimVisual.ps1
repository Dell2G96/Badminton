param(
    [ValidateSet('Host', 'Client')][string]$RenderSide = 'Host',
    [ValidateRange(640,3840)][int]$Width = 1280,
    [ValidateRange(360,2160)][int]$Height = 720,
    [ValidateRange(1024,65535)][int]$Port = 20611,
    [ValidateRange(10,300)][int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$taskProjectRoot = Split-Path -Parent $PSScriptRoot
$taskGameRoot = Join-Path $taskProjectRoot 'Saved/Packages/BadmintonMVP/Windows/FPS_Dell2g'
$taskExecutable = Join-Path $taskGameRoot 'Binaries/Win64/FPS_Dell2g.exe'
$taskScreenshot = Join-Path $taskGameRoot 'Saved/Screenshots/BadmintonPrototype.png'
$taskDate = [TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTime]::UtcNow, 'Korea Standard Time').ToString('yy.MM.dd')
$taskRunId = (Get-Date -Format 'yyyyMMdd-HHmmss') + "-$Port"
$taskOutputFolder = Join-Path $taskProjectRoot "Docs/$taskDate"
$taskOutput = Join-Path $taskOutputFolder "$taskDate - 조준 패키지 $RenderSide ${Width}x${Height} $taskRunId - GPT-6.png"
$taskHostLog = Join-Path $taskProjectRoot "Saved/Logs/BadmintonAimVisualHost-$taskRunId.log"
$taskClientLog = Join-Path $taskProjectRoot "Saved/Logs/BadmintonAimVisualClient-$taskRunId.log"
if (-not (Test-Path -LiteralPath $taskExecutable)) { throw '패키징된 실행 파일이 없습니다.' }
New-Item -ItemType Directory -Path $taskOutputFolder -Force | Out-Null
$taskStarted = [DateTime]::UtcNow
$taskCommon = @('-game', '-unattended', '-nosound', '-nosplash', '-BadmintonAimTest', '-ExecCmds="t.MaxFPS 60"')
$taskRender = @('-RenderOffscreen', '-windowed', "-ResX=$Width", "-ResY=$Height", '-BadmintonCapture', '-BadmintonCaptureOnContact')
$taskHostMode = if ($RenderSide -eq 'Host') { $taskRender } else { @('-nullrhi') }
$taskClientMode = if ($RenderSide -eq 'Client') { $taskRender } else { @('-nullrhi') }
$taskHost = $null
$taskClient = $null
try {
    $taskHostArgs = @('/Game/Badminton/Maps/L_Badminton_Prototype?listen', "-port=$Port") + $taskCommon + $taskHostMode + @('-abslog="' + $taskHostLog + '"')
    $taskHost = Start-Process -FilePath $taskExecutable -ArgumentList $taskHostArgs -PassThru -WindowStyle Hidden
    Write-Output "조준 화면 검사 시작: $RenderSide ${Width}x${Height}, Host PID=$($taskHost.Id)"
    $taskDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        if ($taskHost.HasExited) { throw "호스트 초기화 실패: $taskHostLog" }
        $taskHostText = if (Test-Path -LiteralPath $taskHostLog) { Get-Content -LiteralPath $taskHostLog -Raw } else { '' }
        if ($taskHostText -match 'IpNetDriver listening on port|GameNetDriver.*listening on port') { break }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $taskDeadline)
    if ($taskHostText -notmatch 'listening on port') { throw "호스트 준비 시간 초과: $taskHostLog" }
    $taskClientArgs = @("127.0.0.1:$Port") + $taskCommon + $taskClientMode + @('-abslog="' + $taskClientLog + '"')
    $taskClient = Start-Process -FilePath $taskExecutable -ArgumentList $taskClientArgs -PassThru -WindowStyle Hidden
    $taskDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $taskPassed = $false
    do {
        if ($taskHost.HasExited -or $taskClient.HasExited) { throw '화면 검사 중 게임 프로세스가 종료됐습니다.' }
        $taskHostText = Get-Content -LiteralPath $taskHostLog -Raw
        $taskClientText = if (Test-Path -LiteralPath $taskClientLog) { Get-Content -LiteralPath $taskClientLog -Raw } else { '' }
        $taskCombined = $taskHostText + $taskClientText
        if ($taskCombined -match 'BADMINTON_\w+_TEST FAIL|Fatal error:|Assertion failed:|Ensure condition failed:|LogTemp: Error:') { throw "검사 실패: $taskHostLog / $taskClientLog" }
        $taskFreshImage = (Test-Path -LiteralPath $taskScreenshot) -and (Get-Item -LiteralPath $taskScreenshot).LastWriteTimeUtc -gt $taskStarted
        if ($taskHostText -match 'BADMINTON_AIM_TEST PASS' -and $taskClientText -match 'BADMINTON_AIM_TEST PASS' -and $taskFreshImage) {
            $taskShots = [regex]::Matches($taskHostText, 'BADMINTON_SHOT_ACCEPTED side=(\d) rally=(\d+) sequence=(\d+)')
            if ($taskShots.Count -lt 10) { throw '서버 승인 타격 10회를 확인하지 못했습니다.' }
            for ($taskIndex = 1; $taskIndex -lt 10; $taskIndex++) {
                if ($taskShots[$taskIndex].Groups[2].Value -ne $taskShots[0].Groups[2].Value -or $taskShots[$taskIndex].Groups[1].Value -eq $taskShots[$taskIndex-1].Groups[1].Value) { throw '한 랠리의 10회 교대 타격 검사 실패' }
            }
            Copy-Item -LiteralPath $taskScreenshot -Destination $taskOutput -ErrorAction Stop
            Add-Type -AssemblyName System.Drawing
            $taskImage = [System.Drawing.Image]::FromFile($taskOutput)
            try {
                if ($taskImage.Width -ne $Width -or $taskImage.Height -ne $Height) { throw '캡처 해상도가 요청값과 다릅니다.' }
            } finally { $taskImage.Dispose() }
            $taskPassed = $true
            break
        }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $taskDeadline)
    if (-not $taskPassed) { throw "화면 검사 시간 초과: $taskHostLog / $taskClientLog" }
    Write-Output "BADMINTON_AIM_VISUAL_CAPTURE PASS: $RenderSide ${Width}x${Height}, 양쪽 조준 및 10회 교대 타격"
    Write-Output "화면 내용은 별도 시각 검토 필요: $taskOutput"
    Write-Output "로그: $taskHostLog / $taskClientLog"
} finally {
    foreach ($taskProcess in @($taskClient, $taskHost)) {
        if ($taskProcess -and -not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id -ErrorAction SilentlyContinue; $taskProcess.WaitForExit(10000) | Out-Null }
    }
}
