param([string]$HostLog, [string]$ClientLog, [switch]$SameMachine, [switch]$SelfTest)

$ErrorActionPreference = 'Stop'
$taskCulture = [Globalization.CultureInfo]::InvariantCulture

function Read-Flow([string]$Path) {
    $raw = Get-Content -LiteralPath $Path -Raw
    $end = [regex]::Match($raw, 'BADMINTON_\w+_TEST (?:PASS|FAIL)[^\r\n]*')
    if ($end.Success) { $raw = $raw.Substring(0, $end.Index + $end.Length) }
    $meta = @([regex]::Matches($raw, 'BADMINTON_TRACE_BEGIN run=([A-Fa-f0-9]{32}) clock=(\w+) machine=(\S+) model=(\w+)'))
    if ($meta.Count -ne 1) { throw "PositionTrace의 단일 실행 메타데이터 필요: $Path" }
    $rows = @([regex]::Matches($raw, 'BADMINTON_SHOT_FLOW ([^\r\n]*)') | ForEach-Object {
        $row = @{}
        foreach ($field in [regex]::Matches($_.Groups[1].Value, '(\w+)=([^ ]*)')) { $row[$field.Groups[1].Value] = $field.Groups[2].Value }
        foreach ($name in @('qpc','distance_cm','height_cm')) { $row[$name] = [double]::Parse($row[$name], $taskCulture) }
        foreach ($name in @('auth','side','rally','sequence','shot','key','result')) { $row[$name] = [int]$row[$name] }
        [pscustomobject]$row
    })
    if (!$rows.Count) { throw "타격 흐름 계측 없음: $Path" }
    [pscustomobject]@{Meta=($meta[0].Groups[1..4].Value -join '|'); Rows=$rows; Passed=($raw -match 'BADMINTON_\w+_TEST PASS'); Failed=($raw -match 'BADMINTON_\w+_TEST FAIL')}
}

function Compare-Flow($ServerRows, $ClientRows) {
    $inputRow = $null
    foreach ($row in $ClientRows) {
        if ($row.stage -eq 'Input' -and $row.auth -eq 0) { $inputRow = $row }
        if ($row.stage -ne 'Activate' -or $row.auth -ne 0) { continue }
        # Keys are scoped to a player and a short single-connection run; never guess ambiguous matches.
        $local = @($ClientRows | Where-Object { $_.stage -eq 'Activate' -and $_.side -eq $row.side -and $_.key -eq $row.key -and $_.shot -eq $row.shot })
        $server = @($ServerRows | Where-Object { $_.stage -eq 'Activate' -and $_.auth -eq 1 -and $_.side -eq $row.side -and $_.key -eq $row.key -and $_.shot -eq $row.shot })
        $contact = @($ServerRows | Where-Object { $_.stage -eq 'Contact' -and $_.auth -eq 1 -and $_.side -eq $row.side -and $_.key -eq $row.key -and $_.shot -eq $row.shot })
        $inputOK = $null -ne $inputRow -and $inputRow.side -eq $row.side -and $inputRow.shot -eq $row.shot -and $row.qpc -ge $inputRow.qpc -and $row.qpc - $inputRow.qpc -lt .1
        $status = if ($local.Count -ne 1 -or $server.Count -gt 1 -or $contact.Count -gt 1) {'Ambiguous'} elseif ($server.Count -eq 0) {'NoServerActivation'} elseif ($server[0].qpc -lt $row.qpc) {'InvalidOrder'} elseif ($contact.Count -eq 0) {'NoContact'} elseif ($contact[0].qpc -lt $server[0].qpc) {'InvalidOrder'} elseif ($contact[0].result -eq 1) {'Contact'} else {'RejectedContact'}
        [pscustomobject]@{
            Side=$row.side; Key=$row.key; Rally=$row.rally; Sequence=$row.sequence; Shot=$row.shot; Status=$status
            InputMs=if ($inputOK) {($row.qpc-$inputRow.qpc)*1000.} else {$null}
            TransitMs=if ($status -in @('Contact','RejectedContact','NoContact')) {($server[0].qpc-$row.qpc)*1000.} else {$null}
            ContactMs=if ($status -in @('Contact','RejectedContact')) {($contact[0].qpc-$server[0].qpc)*1000.} else {$null}
            LocalHeight=$row.height_cm; ServerHeight=if ($server.Count -eq 1) {$server[0].height_cm} else {$null}
        }
    }
}

if ($SelfTest) {
    function New-Row($Stage,$Auth,$Time,$Key=1,$Result=-1) { [pscustomobject]@{stage=$Stage;auth=$Auth;qpc=[double]$Time;side=1;shot=1;key=$Key;result=$Result;rally=1;sequence=1;height_cm=200.} }
    $client=@((New-Row Input 0 100),(New-Row Activate 0 100.001))
    $server=@((New-Row Activate 1 100.101),(New-Row Contact 1 100.102 1 1))
    $r=@(Compare-Flow $server $client)[0]
    if ($r.Status -ne 'Contact' -or [Math]::Abs($r.InputMs-1.) -gt .001 -or [Math]::Abs($r.TransitMs-100.) -gt .001 -or [Math]::Abs($r.ContactMs-1.) -gt .001) {throw 'Known timing failed'}
    if (@(Compare-Flow @() $client)[0].Status -ne 'NoServerActivation') {throw 'Missing activation failed'}
    if (@(Compare-Flow ($server + $server) $client)[0].Status -ne 'Ambiguous') {throw 'Duplicate key failed'}
    if (@(Compare-Flow @((New-Row Activate 1 99)) $client)[0].Status -ne 'InvalidOrder') {throw 'Clock order failed'}
    if (@(Compare-Flow @((New-Row Activate 1 100.101),(New-Row Contact 1 100.102 1 0)) $client)[0].Status -ne 'RejectedContact') {throw 'Rejected contact failed'}
    'BADMINTON_SHOT_FLOW_SELFTEST PASS: 알려진 구간 시간, 서버 미관측, 키 중복, 시각 역전, 접촉 거절'
    exit 0
}

if (!$SameMachine) { throw '같은 Windows PC의 단일 연결 실행만 -SameMachine으로 분석하세요.' }
$taskHost = Read-Flow $HostLog
$taskClient = Read-Flow $ClientLog
if ($taskHost.Meta -ne $taskClient.Meta -or $taskHost.Meta -notmatch '\|windows_qpc\|') { throw '실행 GUID·QPC·머신·모델 불일치' }
$taskRows = @(Compare-Flow $taskHost.Rows $taskClient.Rows)
function Format-Number($Value) { if ($null -eq $Value) { return '—' }; ([double]$Value).ToString('F3',$taskCulture) }
$taskDate = [TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTime]::UtcNow,'Korea Standard Time').ToString('yy.MM.dd')
$taskFolder = Join-Path (Split-Path -Parent $PSScriptRoot) "Docs/$taskDate"
New-Item -ItemType Directory -Path $taskFolder -Force | Out-Null
$taskOutput = Join-Path $taskFolder "$taskDate - 타격 흐름 $([IO.Path]::GetFileNameWithoutExtension($HostLog)) - GPT-6.md"
$taskLines = [Collections.Generic.List[string]]::new()
$taskLines.AddRange([string[]]@('# 입력·GAS·서버 접촉 흐름','',"작성일: $taskDate · 모델: GPT-6",'',"실행 메타데이터: $($taskHost.Meta)","검사 마커: 호스트 PASS=$($taskHost.Passed) FAIL=$($taskHost.Failed), 참가자 PASS=$($taskClient.Passed) FAIL=$($taskClient.Failed). 보고서 생성 성공과 게임 검사 성공은 별개다.",'',"호스트: $((Resolve-Path -LiteralPath $HostLog).Path)","참가자: $((Resolve-Path -LiteralPath $ClientLog).Path)",'','| 진영/키 | 로컬 랠리/시퀀스 | 샷 | 상태 | 입력→로컬 활성화(ms) | 로컬→서버 활성화(ms) | 서버 활성화→접촉(ms) | 로컬/서버 높이(cm) |','|---|---|---|---|---:|---:|---:|---|'))
foreach ($r in $taskRows) { $taskLines.Add("| $($r.Side)/$($r.Key) | $($r.Rally)/$($r.Sequence) | $($r.Shot) | $($r.Status) | $(Format-Number $r.InputMs) | $(Format-Number $r.TransitMs) | $(Format-Number $r.ContactMs) | $(Format-Number $r.LocalHeight) / $(Format-Number $r.ServerHeight) |") }
$taskInputs=@($taskClient.Rows | Where-Object {$_.stage -eq 'Input' -and $_.auth -eq 0}).Count
$taskBlocked=@($taskClient.Rows | Where-Object {$_.stage -eq 'Dispatch' -and $_.auth -eq 0 -and $_.result -eq 0}).Count
$taskLines.AddRange([string[]]@('',"참가자 입력 $taskInputs, 발동 수 0인 Dispatch $taskBlocked, 로컬 활성화 $($taskRows.Count). 반복 자동 입력의 차단은 정상 회복 구간도 포함한다.",'','- 각 프로세스 최초 TEST PASS/FAIL까지 집계한다. 서로 다른 PC, 실행, 모델 또는 재접속 로그를 섞지 않는다. 키 재사용·중복은 Ambiguous로 제외한다.','- 로컬→서버 활성화는 전송·재전송·큐·프레임 처리와 서버 GAS 진입까지 포함한다. 순수 편도 전송시간 또는 독립 RTT가 아니다.','- NoServerActivation은 서버 능력 본문 미관측이다. 서버 CanActivate 거절과 실제 미도착을 이 표만으로 구분할 수 없다. 엔진 거절 로그를 함께 확인한다.','- Input은 소프트웨어 SendShot 진입이며 물리 마우스 시각이 아니다. Contact 시간은 TryShot 완료 시점이다. 성공 후 Flight 시퀀스가 증가하므로 서버/클라이언트 시퀀스 동일을 매칭 조건으로 삼지 않는다.','- 선택적 로그만 추가하며 판정·RPC·허용 시간은 바꾸지 않는다. 표본 수가 적은 단일 자동 랠리의 결과를 실전 거절률이나 지연 보장값으로 일반화하지 않는다.'))
[IO.File]::WriteAllText($taskOutput, ($taskLines -join "`r`n")+"`r`n",[Text.UTF8Encoding]::new($false))
"BADMINTON_SHOT_FLOW_REPORT: $taskOutput"
$taskRows | Format-Table Side,Key,Status,TransitMs,ServerHeight
