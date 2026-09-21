param(
    [string]$EngineRoot = 'D:\Game\UE_5.7',
    [string]$GameExecutable,
    [int]$Port = 17777,
    [int]$TimeoutSeconds = 180,
    [ValidateRange(1,100)][int]$MatchCount = 1,
    [ValidateSet('Movement', 'Rally', 'Match', 'Abilities', 'Presentation', 'Aim', 'ThirdPerson')]
    [string]$TestKind = 'Movement',
    [ValidateRange(0, 1000)]
    [int]$OneWayLagMs = 0,
    [ValidateRange(0, 100)]
    [int]$PacketLossPercent = 0,
    [switch]$CheckCapacity,
    [switch]$CheckReconnect,
    [switch]$CheckHostLoss,
    [switch]$ThirdPersonControls,
    [switch]$NetMetrics,
    [switch]$PositionTrace,
    [switch]$ShotFlow,
    [switch]$TransportTrace,
    [ValidateSet('RoundTrip','Legacy')][string]$ClockMode = 'Legacy'
)

$ErrorActionPreference = 'Stop'
if ($ThirdPersonControls -and $TestKind -notin @('Match','Movement','ThirdPerson')) { throw '3인칭 입력 검사는 Match, Movement, ThirdPerson에서 지원합니다.' }
if ($ShotFlow) { $NetMetrics = $true; $PositionTrace = $true }
if ($TransportTrace) { $ShotFlow = $true; $NetMetrics = $true; $PositionTrace = $true }
if ($PositionTrace -and ($CheckReconnect -or $CheckHostLoss -or $CheckCapacity)) { throw '위치 비교는 단일 호스트·참가자 실행으로 수행하세요.' }
$taskProjectRoot = Split-Path -Parent $PSScriptRoot
$taskProject = Join-Path $taskProjectRoot 'FPS_Dell2g.uproject'
$taskEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$taskPrefix = @('"' + $taskProject + '"')
if ($GameExecutable) {
    $taskEditor = (Resolve-Path -LiteralPath $GameExecutable).Path
    $taskPrefix = @()
}
$taskLogRoot = Join-Path $taskProjectRoot 'Saved\Logs'
New-Item -ItemType Directory -Path $taskLogRoot -Force | Out-Null
$taskRunId = (Get-Date -Format 'yyyyMMdd-HHmmss') + "-$Port"
$taskHostLog = Join-Path $taskLogRoot "BadmintonHost-$taskRunId.log"
$taskClientLog = Join-Path $taskLogRoot "BadmintonClient-$taskRunId.log"
$taskTestFlag = switch ($TestKind) { ThirdPerson { '-BadmintonThirdPersonTest' } Movement { '-BadmintonNetTest' } Rally { '-BadmintonShotTest' } Match { '-BadmintonMatchTest' } Abilities { '-BadmintonAbilityTest' } Presentation { '-BadmintonPresentationTest' } Aim { '-BadmintonAimTest' } }
$taskMarker = switch ($TestKind) { ThirdPerson { 'BADMINTON_THIRD_PERSON_TEST' } Movement { 'BADMINTON_NET_TEST' } Rally { 'BADMINTON_SHOT_TEST' } Match { 'BADMINTON_MATCH_TEST' } Abilities { 'BADMINTON_ABILITY_TEST' } Presentation { 'BADMINTON_PRESENTATION_TEST' } Aim { 'BADMINTON_AIM_TEST' } }
$taskCommon = @('-game', '-nullrhi', '-unattended', '-nosound', '-nosplash', $taskTestFlag, "-BadmintonMatchCount=$MatchCount", "-PktLag=$OneWayLagMs", "-PktLoss=$PacketLossPercent",
    '-ddc=NoZenLocalFallback', "-LocalDataCachePath=$taskProjectRoot\DerivedDataCache", '-ExecCmds="t.MaxFPS 60"')
if($TestKind -ne 'ThirdPerson' -and -not $ThirdPersonControls){ $taskCommon += '-BadmintonLegacyFirstPerson' }
$taskHost = $null
if ($TransportTrace) {
    $taskCommon = @($taskCommon | Where-Object { $_ -notlike '-ExecCmds=*' })
    $taskCommon += @('-ExecCmds="t.MaxFPS 60,net.Reliable.Debug 1"', '-LogCmds="LogNetTraffic Log,LogAbilitySystem Verbose"')
}
if ($NetMetrics) { $taskCommon += '-BadmintonNetMetrics' }
if ($ClockMode -eq 'Legacy') { $taskCommon += '-BadmintonLegacyClock' }
if ($ClockMode -eq 'RoundTrip') { $taskCommon += '-BadmintonRoundTripClock' }
if ($PositionTrace) { $taskCommon += '-BadmintonTraceRun=' + [guid]::NewGuid().ToString('N') }
$taskClient = $null
$taskExtraClient = $null

try {
    $taskHostArgs = $taskPrefix + @("/Game/Badminton/Maps/L_Badminton_Prototype?listen", "-port=$Port") + $taskCommon + @('-abslog="' + $taskHostLog + '"')
    $taskHost = Start-Process -FilePath $taskEditor -ArgumentList $taskHostArgs -PassThru -WindowStyle Hidden
    Write-Output "호스트 시작: PID=$($taskHost.Id), 로그=$taskHostLog"
    $taskDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        if ($taskHost.HasExited) { throw '호스트가 초기화 중 종료되었습니다.' }
        $taskHostText = if (Test-Path -LiteralPath $taskHostLog) { Get-Content -LiteralPath $taskHostLog -Raw } else { '' }
        if ($taskHostText -match 'GameNetDriver.*listening on port|IpNetDriver listening on port') { break }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $taskDeadline)
    if ($taskHostText -notmatch 'listening on port') { throw "호스트 시작 시간 초과: $taskHostLog" }

    $taskClientArgs = $taskPrefix + @("127.0.0.1:$Port") + $taskCommon + @('-abslog="' + $taskClientLog + '"')
    $taskClient = Start-Process -FilePath $taskEditor -ArgumentList $taskClientArgs -PassThru -WindowStyle Hidden
    Write-Output "참가자 시작: PID=$($taskClient.Id), 로그=$taskClientLog"
    $taskDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        if ($taskHost.HasExited -or $taskClient.HasExited) { throw '검증 중 프로세스가 종료되었습니다.' }
        $taskHostText = Get-Content -LiteralPath $taskHostLog -Raw
        $taskClientText = if (Test-Path -LiteralPath $taskClientLog) { Get-Content -LiteralPath $taskClientLog -Raw } else { '' }
        if ($taskHostText -match "$taskMarker FAIL" -or $taskClientText -match "$taskMarker FAIL") { throw '네트워크 동작 검증 실패. 로그를 확인하세요.' }
        if ($taskHostText -match "$taskMarker PASS" -and $taskClientText -match "$taskMarker PASS") {
            [regex]::Matches($taskHostText + $taskClientText, "$taskMarker PASS[^\r\n]*") | ForEach-Object { $_.Value }
            if ($TestKind -in @('Rally', 'Presentation', 'Aim')) {
                $taskShots = [regex]::Matches($taskHostText, 'BADMINTON_SHOT_ACCEPTED side=(\d) rally=(\d+) sequence=(\d+)')
                if ($taskShots.Count -lt 10) { throw '서버에서 승인한 타격 수가 부족합니다.' }
                $taskFirstRally = $taskShots[0].Groups[2].Value
                for ($taskIndex = 1; $taskIndex -lt 10; ++$taskIndex) {
                    if ($taskShots[$taskIndex].Groups[2].Value -ne $taskFirstRally -or $taskShots[$taskIndex].Groups[1].Value -eq $taskShots[$taskIndex - 1].Groups[1].Value) {
                        throw '한 랠리에서 양쪽이 교대로 10회 타격하지 못했습니다.'
                    }
                }
            }
            Write-Output "독립 프로세스 검증 통과: $TestKind, 편도 지연=$OneWayLagMs ms, 송신 패킷 손실=$PacketLossPercent %"
            if ($TestKind -eq 'Match') {
                foreach ($taskSide in @(0,1)) {
                    $taskObserved = [regex]::Matches($taskHostText + $taskClientText, "BADMINTON_MATCH_FINISH_OBSERVED side=$taskSide score=11:0 match=(\d+)")
                    if ($taskObserved.Count -ne $MatchCount) { throw "Side $taskSide 경기 종료 확인 횟수 불일치" }
                    for ($taskMatchIndex = 0; $taskMatchIndex -lt $MatchCount; $taskMatchIndex++) {
                        if ([int]$taskObserved[$taskMatchIndex].Groups[1].Value -ne ($taskMatchIndex+1)) { throw '경기 순서 누락 또는 중복' }
                    }
                }
                $taskPoints = [regex]::Matches($taskHostText, 'BADMINTON_POINT rally=(\d+)')
                if ($taskPoints.Count -ne ($MatchCount*11) -or ($taskPoints | ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique).Count -ne $taskPoints.Count) { throw '득점 수 또는 랠리별 중복 득점 검사 실패' }
                Write-Output "BADMINTON_MATCH_SERIES PASS: $MatchCount 경기 종료와 재경기, $($taskPoints.Count)개 고유 랠리 득점"
            }
            if ($CheckCapacity) {
                $taskExtraLog = Join-Path $taskLogRoot "BadmintonCapacity-$taskRunId.log"
                $taskExtraArgs = $taskPrefix + @("127.0.0.1:$Port") + @($taskCommon | Where-Object { $_ -ne $taskTestFlag }) + @('-abslog="' + $taskExtraLog + '"')
                $taskExtraClient = Start-Process -FilePath $taskEditor -ArgumentList $taskExtraArgs -PassThru -WindowStyle Hidden
                $taskCapacityDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
                $taskRejected = $false
                do {
                    $taskExtraText = if (Test-Path -LiteralPath $taskExtraLog) { Get-Content -LiteralPath $taskExtraLog -Raw } else { '' }
                    if ($taskExtraText -match 'BadmintonRoomFull') { $taskRejected = $true; break }
                    if ($taskExtraClient.HasExited) { break }
                    Start-Sleep -Milliseconds 500
                } while ((Get-Date) -lt $taskCapacityDeadline)
                if (-not $taskRejected) { throw '세 번째 참가자의 정원 초과 거절을 확인하지 못했습니다.' }
                Write-Output 'BADMINTON_CAPACITY_TEST PASS: 세 번째 참가자를 BadmintonRoomFull로 거절했습니다.'
                if (-not $taskExtraClient.HasExited) { Stop-Process -Id $taskExtraClient.Id; $taskExtraClient.WaitForExit() }
            }
            if ($CheckReconnect) {
                $taskBeforeLeave = (Get-Content -LiteralPath $taskHostLog -Raw).Length
                Stop-Process -Id $taskClient.Id
                $taskClient.WaitForExit()
                $taskLeaveDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
                do {
                    $taskLatest = Get-Content -LiteralPath $taskHostLog -Raw
                    if ($taskLatest.Substring([Math]::Min($taskBeforeLeave, $taskLatest.Length)) -match 'BADMINTON Lobby players=1 phase=0') { break }
                    Start-Sleep -Milliseconds 500
                } while ((Get-Date) -lt $taskLeaveDeadline)
                if ($taskLatest.Substring([Math]::Min($taskBeforeLeave, $taskLatest.Length)) -notmatch 'BADMINTON Lobby players=1 phase=0') { throw '참가자 이탈 후 대기 상태 복귀 실패' }
                $taskBeforeJoin = $taskLatest.Length
                $taskRejoinLog = Join-Path $taskLogRoot "BadmintonRejoin-$taskRunId.log"
                $taskRejoinArgs = $taskPrefix + @("127.0.0.1:$Port") + @($taskCommon | Where-Object { $_ -ne $taskTestFlag }) + @('-abslog="' + $taskRejoinLog + '"')
                $taskClient = Start-Process -FilePath $taskEditor -ArgumentList $taskRejoinArgs -PassThru -WindowStyle Hidden
                $taskClientLog = $taskRejoinLog
                $taskJoinDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
                do {
                    $taskLatest = Get-Content -LiteralPath $taskHostLog -Raw
                    if ($taskLatest.Substring([Math]::Min($taskBeforeJoin, $taskLatest.Length)) -match 'BADMINTON Lobby players=2 phase=1') { break }
                    if ($taskClient.HasExited) { throw '재접속 프로세스 종료' }
                    Start-Sleep -Milliseconds 500
                } while ((Get-Date) -lt $taskJoinDeadline)
                if ($taskLatest.Substring([Math]::Min($taskBeforeJoin, $taskLatest.Length)) -notmatch 'BADMINTON Lobby players=2 phase=1') { throw '재접속 후 준비 대기 상태 복귀 실패' }
                Write-Output 'BADMINTON_REJOIN_TEST PASS: 참가자 이탈 후 1인 대기, 재접속 후 2인 준비 대기 확인'
            }
            if ($CheckHostLoss) {
                $taskBeforeHostLoss = (Get-Content -LiteralPath $taskClientLog -Raw).Length
                Stop-Process -Id $taskHost.Id
                $taskHost.WaitForExit()
                $taskLossDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
                $taskRecovered = $false
                do {
                    if ($taskClient.HasExited) { throw '호스트 종료 후 참가자 프로세스가 종료되었습니다.' }
                    $taskLatestClient = Get-Content -LiteralPath $taskClientLog -Raw
                    $taskLossText = $taskLatestClient.Substring([Math]::Min($taskBeforeHostLoss, $taskLatestClient.Length))
                    $taskLocalLobby = $taskLossText -match 'BADMINTON Lobby players=1 phase=0' -or
                        ($taskLossText -match 'BADMINTON_AI_SPAWNED side=1 pawn=1' -and $taskLossText -match 'BADMINTON Lobby players=2 phase=1')
                    $taskRecovered = $taskLossText -match 'NetworkFailure' -and
                        $taskLossText -match 'LoadMap: /Game/Badminton/Maps/L_Badminton_Prototype\?closed' -and $taskLocalLobby
                    if ($taskRecovered) { break }
                    Start-Sleep -Milliseconds 500
                } while ((Get-Date) -lt $taskLossDeadline)
                if (-not $taskRecovered) { throw '호스트 종료 후 오프라인 코트 복귀를 확인하지 못했습니다.' }
                Write-Output 'BADMINTON_HOST_LOSS_TEST PASS: 호스트 종료 후 참가자가 오프라인 코트로 복귀'
            }
            if ($NetMetrics) {
                & (Join-Path $PSScriptRoot 'Measure-BadmintonNetwork.ps1') -HostLog $taskHostLog -ClientLog $taskClientLog -OneWayLagMs $OneWayLagMs -PacketLossPercent $PacketLossPercent -RequireFlightSamples:($TestKind -in @('Rally','Aim','Presentation'))
            }
            if ($PositionTrace) {
                & (Join-Path $PSScriptRoot 'Measure-BadmintonPosition.ps1') -HostLog $taskHostLog -ClientLog $taskClientLog -SameMachine -OneWayLagMs $OneWayLagMs -PacketLossPercent $PacketLossPercent
            }
            exit 0
        }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $taskDeadline)
    throw '네트워크 검증 시간 초과. 호스트·참가자 로그를 확인하세요.'
}
finally {
    # Only terminate processes started by this test.
    foreach ($taskProcess in @($taskExtraClient, $taskClient, $taskHost)) {
        if ($null -ne $taskProcess -and -not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id }
    }
    if ($ShotFlow -and (Test-Path -LiteralPath $taskHostLog) -and (Test-Path -LiteralPath $taskClientLog)) {
        try {
            & (Join-Path $PSScriptRoot 'Measure-BadmintonShotFlow.ps1') -HostLog $taskHostLog -ClientLog $taskClientLog -SameMachine
        } catch {
            Write-Warning "타격 흐름 보고서 생성 실패(게임 검사 결과와 별도): $($_.Exception.Message)"
        }
    }
}
