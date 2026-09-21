param(
    [string]$HostLog,
    [string]$ClientLog,
    [ValidateRange(0,1000)][int]$OneWayLagMs = 0,
    [ValidateRange(0,100)][int]$PacketLossPercent = 0,
    [switch]$RequireFlightSamples,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'
$taskCulture = [Globalization.CultureInfo]::InvariantCulture
$taskNumber = '(-?\d+(?:\.\d+)?)'
$taskFlightPattern = 'BADMINTON_METRIC_FLIGHT rally=\d+ sequence=\d+ age_ms=' + $taskNumber + ' gap_cm=' + $taskNumber + ' step_cm=' + $taskNumber + ' residual_cm=' + $taskNumber + ' dt_ms=' + $taskNumber + ' engine_ping_ms=' + $taskNumber + ' capped=([01])'
$taskFlightPattern += '(?: receipt_ms=' + $taskNumber + ' present_ms=' + $taskNumber + ' floor=([01]))?'
$taskFlightPattern += '(?: sync_age_ms=' + $taskNumber + ' clock_rtt_ms=' + $taskNumber + ' clock_synced=([01]))?'

function Get-MetricSummary([double[]]$Values) {
    $valid = @($Values | Where-Object { -not [double]::IsNaN($_) -and -not [double]::IsInfinity($_) } | Sort-Object)
    if (-not $valid.Count) { return [pscustomobject]@{Count=0; Median=$null; P95=$null; Max=$null} }
    $middle = [int][Math]::Floor($valid.Count / 2)
    $median = if ($valid.Count % 2) { $valid[$middle] } else { ($valid[$middle-1] + $valid[$middle]) / 2 }
    return [pscustomobject]@{Count=$valid.Count; Median=$median; P95=$valid[[int][Math]::Ceiling($valid.Count * .95)-1]; Max=$valid[-1]}
}

function Format-Number($Value) {
    if ($null -eq $Value) { return '미측정' }
    return ([double]$Value).ToString('0.000', [Globalization.CultureInfo]::InvariantCulture)
}

if ($SelfTest) {
    $odd = Get-MetricSummary @(100,3,2,1,4)
    $even = Get-MetricSummary @(4,1,3,2)
    $invalid = Get-MetricSummary @([double]::NaN,[double]::PositiveInfinity,7)
    $empty = Get-MetricSummary @()
    $hundred = Get-MetricSummary (1..100)
    if ($odd.Median -ne 3 -or $odd.P95 -ne 100 -or $odd.Max -ne 100 -or $even.Median -ne 2.5 -or $even.P95 -ne 4 -or $hundred.Median -ne 50.5 -or $hundred.P95 -ne 95 -or $invalid.Count -ne 1 -or $invalid.Median -ne 7 -or $empty.Count -ne 0 -or $null -ne $empty.Median) { throw '백분위·빈 자료·비유한값 처리 검사 실패' }
    $legacy = 'BADMINTON_METRIC_FLIGHT rally=1 sequence=2 age_ms=-10.000 gap_cm=1.000 step_cm=2.000 residual_cm=3.000 dt_ms=16.667 engine_ping_ms=100.000 capped=0'
    $legacyMatch = [regex]::Match($legacy, $taskFlightPattern)
    $currentMatch = [regex]::Match($legacy + ' receipt_ms=16.667 present_ms=16.667 floor=1', $taskFlightPattern)
    if (-not $legacyMatch.Success -or $legacyMatch.Groups[8].Success -or $currentMatch.Groups[8].Value -ne '16.667' -or $currentMatch.Groups[9].Value -ne '16.667' -or $currentMatch.Groups[10].Value -ne '1') { throw '이전/신규 계측 로그 형식 호환 검사 실패' }
    $clockMatch = [regex]::Match($legacy + ' receipt_ms=16.667 present_ms=70.000 floor=0 sync_age_ms=70.000 clock_rtt_ms=140.000 clock_synced=1', $taskFlightPattern)
    if ($clockMatch.Groups[11].Value -ne '70.000' -or $clockMatch.Groups[12].Value -ne '140.000' -or $clockMatch.Groups[13].Value -ne '1' -or $currentMatch.Groups[13].Success) { throw '왕복 시계 형식과 이전 로그 호환 검사 실패' }
    Write-Output 'BADMINTON_METRICS_SUMMARY_TEST PASS: 홀짝 중앙값, nearest-rank p95, 빈 자료, 비유한값 제외'
    Write-Output 'BADMINTON_METRICS_FORMAT_TEST PASS: 이전/수신 시간 보완 형식과 선택 필드 구분'
    Write-Output 'BADMINTON_METRICS_CLOCK_FORMAT_TEST PASS: 왕복 시계 선택 지표와 이전 형식 구분'
    return
}

if (-not $HostLog -or -not $ClientLog) { throw 'HostLog와 ClientLog가 필요합니다.' }
$taskHostPath = (Resolve-Path -LiteralPath $HostLog).Path
$taskClientPath = (Resolve-Path -LiteralPath $ClientLog).Path
if ($taskHostPath -eq $taskClientPath) { throw '호스트와 참가자 로그는 서로 달라야 합니다.' }
$taskHostText = Get-Content -LiteralPath $taskHostPath -Raw
$taskClientText = Get-Content -LiteralPath $taskClientPath -Raw
# Stop at each process's first probe completion. Re-reading after shutdown must not
# add incidental frames that ran while the other process/report was finishing.
foreach ($taskName in @('taskHostText','taskClientText')) {
    $taskText = Get-Variable -Name $taskName -ValueOnly
    $taskEnd = [regex]::Match($taskText, 'BADMINTON_\w+_TEST (?:PASS|FAIL)[^\r\n]*')
    if ($taskEnd.Success) { Set-Variable -Name $taskName -Value $taskText.Substring(0, $taskEnd.Index + $taskEnd.Length) }
}
$taskSamples = @([regex]::Matches($taskClientText, $taskFlightPattern) | ForEach-Object {
    [pscustomobject]@{
        Age=[double]::Parse($_.Groups[1].Value,$taskCulture); Gap=[double]::Parse($_.Groups[2].Value,$taskCulture)
        Step=[double]::Parse($_.Groups[3].Value,$taskCulture); Residual=[double]::Parse($_.Groups[4].Value,$taskCulture)
        Delta=[double]::Parse($_.Groups[5].Value,$taskCulture); Ping=[double]::Parse($_.Groups[6].Value,$taskCulture)
        Capped=$_.Groups[7].Value -eq '1'
        Receipt=if ($_.Groups[8].Success) { [double]::Parse($_.Groups[8].Value,$taskCulture) } else { $null }
        Presentation=if ($_.Groups[9].Success) { [double]::Parse($_.Groups[9].Value,$taskCulture) } else { $null }
        ReceiptFloor=$_.Groups[10].Success -and $_.Groups[10].Value -eq '1'
        SyncAge=if ($_.Groups[11].Success) { [double]::Parse($_.Groups[11].Value,$taskCulture) } else { $null }
        ClockRTT=if ($_.Groups[12].Success) { [double]::Parse($_.Groups[12].Value,$taskCulture) } else { $null }
        ClockSynced=$_.Groups[13].Success -and $_.Groups[13].Value -eq '1'
    }
})
$taskContacts = @([regex]::Matches($taskHostText, 'BADMINTON_METRIC_CONTACT side=(-?\d+) rally=-?\d+ shot=\d+ accepted=([01]) reason=(\w+)'))
$taskResets = @([regex]::Matches($taskClientText, 'BADMINTON_METRIC_RESET rally=\d+ sequence=\d+ flying=([01]) snap_cm=' + $taskNumber))
if ($RequireFlightSamples -and ($taskSamples.Count -lt 10 -or $taskContacts.Count -lt 10)) { throw '비행/접촉 계측 표본이 부족합니다. 성공한 랠리와 계측 플래그를 확인하세요.' }

$taskRoot = Split-Path -Parent $PSScriptRoot
$taskDate = [TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTime]::UtcNow, 'Korea Standard Time').ToString('yy.MM.dd')
$taskFolder = Join-Path $taskRoot "Docs/$taskDate"
New-Item -ItemType Directory -Path $taskFolder -Force | Out-Null
$taskRun = [IO.Path]::GetFileNameWithoutExtension($taskHostPath) -replace '[^A-Za-z0-9_-]', '_'
$taskReport = Join-Path $taskFolder "$taskDate - 네트워크 계측 $taskRun - GPT-6.md"
$taskLines = [Collections.Generic.List[string]]::new()
$taskLines.Add('# 배드민턴 네트워크 계측 결과')
$taskLines.Add('')
$taskLines.Add("작성일: $taskDate · 모델: GPT-6 · 편도 설정 $OneWayLagMs ms · 송신 손실 설정 $PacketLossPercent %")
$taskLines.Add('')
$taskLines.Add('호스트의 서버 접촉 판정과 참가자의 로컬 표시 계측이다. 자동 검사 성공 여부는 원래 테스트 로그와 함께 판단한다. 이 보고서 자체가 경기나 외부망 검증 성공을 뜻하지 않는다.')
$taskLines.Add('')
$taskFailures = [regex]::Matches($taskHostText + $taskClientText, 'BADMINTON_\w+_TEST FAIL').Count
$taskHostPasses = [regex]::Matches($taskHostText, 'BADMINTON_\w+_TEST PASS').Count
$taskClientPasses = [regex]::Matches($taskClientText, 'BADMINTON_\w+_TEST PASS').Count
$taskLines.Add("원본 검사 마커: FAIL $taskFailures 개, 호스트 PASS $taskHostPasses 개, 참가자 PASS $taskClientPasses 개. 여러 검사 마커가 한 실행에 포함될 수 있다.")
$taskLines.Add('')
$taskLines.Add("호스트 로그: ``$taskHostPath``")
$taskLines.Add("참가자 로그: ``$taskClientPath``")
$taskLines.Add('')
$taskLines.Add('## 참가자 비행 표시 통계')
$taskLines.Add('')
$taskLines.Add('| 지표 | 표본 수 | 중앙값 | 상위 95% | 최댓값 |')
$taskLines.Add('|---|---:|---:|---:|---:|')
$taskMetrics = @(
    @{Name='상태 나이(ms)'; Values=@($taskSamples | ForEach-Object {$_.Age})},
    @{Name='표시용 시계 기준 상태 나이(ms)'; Values=@($taskSamples | Where-Object {$null -ne $_.SyncAge} | ForEach-Object {$_.SyncAge})},
    @{Name='표시 시계가 선택한 왕복 시간(ms)'; Values=@($taskSamples | Where-Object {$_.ClockSynced} | ForEach-Object {$_.ClockRTT})},
    @{Name='수신 후 경과 시간(ms, 신규 로그)'; Values=@($taskSamples | Where-Object {$null -ne $_.Receipt} | ForEach-Object {$_.Receipt})},
    @{Name='실제 외삽 시간(ms, 신규 로그)'; Values=@($taskSamples | Where-Object {$null -ne $_.Presentation} | ForEach-Object {$_.Presentation})},
    @{Name='보간 전 목표와 거리(cm)'; Values=@($taskSamples | ForEach-Object {$_.Gap})},
    @{Name='샘플 프레임의 표시 이동량(cm)'; Values=@($taskSamples | ForEach-Object {$_.Step})},
    @{Name='보간 후 목표와 잔여 거리(cm)'; Values=@($taskSamples | ForEach-Object {$_.Residual})},
    @{Name='샘플 프레임 DeltaTime(ms)'; Values=@($taskSamples | ForEach-Object {$_.Delta})},
    @{Name='엔진 Ping(ms, 양수만)'; Values=@($taskSamples | Where-Object {$_.Ping -gt 0} | ForEach-Object {$_.Ping})},
    @{Name='샷 전환 위치 설정 거리(cm)'; Values=@($taskResets | Where-Object {$_.Groups[1].Value -eq '1'} | ForEach-Object {[double]::Parse($_.Groups[2].Value,$taskCulture)})},
    @{Name='서브 대기·정지 위치 설정 거리(cm)'; Values=@($taskResets | Where-Object {$_.Groups[1].Value -eq '0'} | ForEach-Object {[double]::Parse($_.Groups[2].Value,$taskCulture)})}
)
foreach ($taskMetric in $taskMetrics) {
    $taskSummary = Get-MetricSummary $taskMetric.Values
    $taskLines.Add("| $($taskMetric.Name) | $($taskSummary.Count) | $(Format-Number $taskSummary.Median) | $(Format-Number $taskSummary.P95) | $(Format-Number $taskSummary.Max) |")
}
$taskCapped = @($taskSamples | Where-Object {$_.Capped}).Count
$taskNegativeAge = @($taskSamples | Where-Object {$_.Age -lt 0}).Count
$taskUnknownPing = @($taskSamples | Where-Object {$_.Ping -le 0}).Count
$taskLines.Add('')
$taskLines.Add("외삽 상한 150ms 초과 표본: $taskCapped / $($taskSamples.Count). 음수 상태 나이: $taskNegativeAge. 엔진 Ping 미확정 표본: $taskUnknownPing.")
$taskNewSamples = @($taskSamples | Where-Object {$null -ne $_.Presentation})
$taskFloorCount = @($taskNewSamples | Where-Object {$_.ReceiptFloor}).Count
$taskRecovered = @($taskNewSamples | Where-Object {$_.Age -le 0 -and $_.Presentation -gt 0}).Count
$taskLines.Add("수신 시간 보완 형식 표본: $($taskNewSamples.Count). 수신 시간 하한 적용: $taskFloorCount. 엔진 원시 나이 0 이하에서 양수 외삽으로 진행한 표본: $taskRecovered. 이전 형식 로그의 추가 지표는 미측정으로 둔다.")
$taskLines.Add("왕복 표시 시계 사용 표본: $(@($taskSamples | Where-Object {$_.ClockSynced}).Count) / $($taskSamples.Count). 미확정/만료 또는 Legacy는 기존 엔진 추정과 수신 시간 하한을 사용한다.")
$taskLines.Add('')
$taskLines.Add('## 서버 접촉 판정')
$taskLines.Add('')
$taskLines.Add('| 진영 | 판정 수 | 접촉 성공 | 접촉 실패 | 실패 비율(%) |')
$taskLines.Add('|---|---:|---:|---:|---:|')
foreach ($taskSide in @(0,1)) {
    $taskSideContacts = @($taskContacts | Where-Object {$_.Groups[1].Value -eq "$taskSide"})
    $taskAccepted = @($taskSideContacts | Where-Object {$_.Groups[2].Value -eq '1'}).Count
    $taskRejected = $taskSideContacts.Count - $taskAccepted
    $taskRate = if ($taskSideContacts.Count) { 100.0 * $taskRejected / $taskSideContacts.Count } else { $null }
    $taskLines.Add("| $taskSide | $($taskSideContacts.Count) | $taskAccepted | $taskRejected | $(Format-Number $taskRate) |")
}
$taskLines.Add('')
$taskLines.Add('| 진영 | 첫 실패 조건 | 횟수 |')
$taskLines.Add('|---|---|---:|')
$taskGroups = @($taskContacts | Where-Object {$_.Groups[2].Value -eq '0'} | Group-Object { $_.Groups[1].Value + '/' + $_.Groups[3].Value })
foreach ($taskGroup in ($taskGroups | Sort-Object Name)) {
    $taskParts = $taskGroup.Name.Split('/')
    $taskLines.Add("| $($taskParts[0]) | $($taskParts[1]) | $($taskGroup.Count) |")
}
if (-not $taskGroups.Count) { $taskLines.Add('| — | 기록된 접촉 실패 없음 | 0 |') }
$taskLines.Add('')
$taskLines.Add('## 해석 범위')
$taskLines.Add('')
$taskLines.Add('- 비행 표본은 비행 중 약 10Hz로 기록한 프레임이다. p95는 정렬 후 ceil(0.95×N)번째 값, 짝수 표본의 중앙값은 가운데 두 값의 평균이다.')
$taskLines.Add('- 자동 검사 로그는 각 프로세스의 첫 TEST PASS/FAIL까지로 계측 구간을 고정한다. 종료 후 같은 로그를 다시 분석해도 종료 대기 중 추가 프레임이 통계에 섞이지 않는다. 완료 마커가 없는 수동 실행 로그는 읽은 전체 구간이다.')
$taskLines.Add('- 목표는 마지막 서버 상태를 최대 150ms 외삽한 로컬 계산값이다. 수신 시간 보완은 max(표시용 서버 추정 시각−상태 시각, 수신 후 로컬 게임 경과 시간, 0)을 사용한다. 가장 오래된 형식은 엔진 서버 추정만 사용했다. 거리 수치는 같은 순간의 실제 서버 좌표와 직접 비교한 오차가 아니다.')
$taskLines.Add('- 표시 이동량에는 정상적인 셔틀 이동도 포함된다. 네트워크 때문에 발생한 추가 보정량만 분리한 수치는 아니다.')
$taskLines.Add('- 상태 나이는 서버 시간 추정값에서 상태 타임스탬프를 뺀 값이다. 시계 보정의 영향을 받으며 실제 편도 지연과 다르다. 음수도 숨기지 않는다.')
$taskLines.Add('- 수신 시간 하한은 받은 이후의 시간만 보장한다. 전송 지연을 추정하거나 서버 시계를 동기화하지 않는다. 신규 capped는 두 시간의 최댓값이 150ms를 넘는지 표시한다.')
$taskLines.Add('- age_ms는 엔진 GameState 기준 원시 값을 유지한다. sync_age_ms는 이번 프레임에 선택한 표시용 시계 기준이다. floor는 선택한 시계보다 수신 후 경과 시간이 더 커서 하한을 사용했는지 뜻한다. 왕복 시계는 대칭 전송 가정과 프레임 큐 오차를 포함하며 서버 판정에는 사용하지 않는다.')
$taskLines.Add('- 엔진 Ping은 PlayerState.GetPingInMilliseconds의 평활된 값이다. 0 또는 미확정은 -1로 기록하고 통계에서 제외한다. 실시간 독립 RTT 프로브가 아니다.')
$taskLines.Add('- 샷 전환/정지 위치 설정은 일반 비행 표본과 분리했다. 첫 상태 초기 배치는 제외했고 서브 재배치 거리에는 의도한 게임 전환도 포함된다.')
$taskLines.Add('- 접촉 판정 분모는 서버 GameMode.TryShot에 도달한 호출이다. GAS 발동 전 차단·스태미나 부족·잘못된 TargetData 등과 전체 마우스 입력 횟수는 포함하지 않는다. 실패 조건이 여러 개면 코드 검사 순서상 첫 조건만 기록한다.')
$taskLines.Add('- 계측 로그는 -BadmintonNetMetrics를 준 개발 실행에서만 활성화된다. 별도의 RoundTrip 표시 시계는 활성화된 경우 약 1초 간격의 요청/응답 RPC를 사용하며, 로그 옵션이 이 RPC를 활성화하는 것은 아니다. 현재 일반 실행은 Legacy이고 개발 비교는 -BadmintonRoundTripClock으로 선택한다. EOS 인증 정보와 원본 로그 전체는 보고서에 복사하지 않는다.')
[IO.File]::WriteAllLines($taskReport, $taskLines, [Text.UTF8Encoding]::new($false))
Write-Output "BADMINTON_METRICS_REPORT: $taskReport"
Write-Output "비행 표본=$($taskSamples.Count), 서버 접촉 판정=$($taskContacts.Count), 외삽 상한 초과=$taskCapped"
