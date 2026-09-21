param(
    [string]$HostLog,
    [string]$ClientLog,
    [switch]$SameMachine,
    [ValidateRange(0,1000)][int]$OneWayLagMs = 0,
    [ValidateRange(0,100)][int]$PacketLossPercent = 0,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'
$taskCulture = [Globalization.CultureInfo]::InvariantCulture

function New-Position($Time, $X, $Y=0, $Z=0, $Sequence=1, $Segment=1, $Flying=1, $Rally=1) {
    [pscustomobject]@{T=[double]$Time; X=[double]$X; Y=[double]$Y; Z=[double]$Z; Sequence=[int]$Sequence; Segment=[int]$Segment; Flying=[int]$Flying; Rally=[int]$Rally}
}

function Test-Segment($A, $B) {
    return $A.Flying -eq 1 -and $B.Flying -eq 1 -and $A.Rally -eq $B.Rally -and $A.Sequence -eq $B.Sequence -and $A.Segment -eq $B.Segment -and ($B.T-$A.T) -gt 0 -and ($B.T-$A.T) -le .05
}

function Get-PositionComparison([object[]]$Server, [object[]]$Client) {
    $rows = [Collections.Generic.List[object]]::new()
    $outside=0; $boundary=0; $mismatch=0; $noHistory=0; $index=0
    foreach ($c in $Client) {
        if ($c.Flying -ne 1) { continue }
        while ($index+1 -lt $Server.Count -and $Server[$index+1].T -le $c.T) { $index++ }
        if ($index+1 -ge $Server.Count -or $c.T -lt $Server[$index].T) { $outside++; continue }
        $a=$Server[$index]; $b=$Server[$index+1]
        if (-not (Test-Segment $a $b)) { $boundary++; continue }
        if ($c.Rally -ne $a.Rally -or $c.Sequence -ne $a.Sequence) { $mismatch++; continue }
        $alpha=($c.T-$a.T)/($b.T-$a.T)
        $dx=$c.X-($a.X+($b.X-$a.X)*$alpha)
        $dy=$c.Y-($a.Y+($b.Y-$a.Y)*$alpha)
        $dz=$c.Z-($a.Z+($b.Z-$a.Z)*$alpha)
        $errorCm=[Math]::Sqrt($dx*$dx+$dy*$dy+$dz*$dz)
        $best=[double]::PositiveInfinity; $lag=$null
        # Search backwards, preserving the latest time in case of equal minima.
        for ($j=$index; $j -ge 0; $j--) {
            $p=$Server[$j]; $q=$Server[$j+1]
            if ($q.T -lt $c.T-.3) { break }
            if (-not (Test-Segment $p $q) -or $p.Rally -ne $c.Rally -or $p.Sequence -ne $c.Sequence -or $p.Segment -ne $a.Segment) { continue }
            $vx=$q.X-$p.X; $vy=$q.Y-$p.Y; $vz=$q.Z-$p.Z
            $norm=$vx*$vx+$vy*$vy+$vz*$vz
            if ($norm -lt .000001) { continue }
            $minAlpha=[Math]::Max(0.0,($c.T-.3-$p.T)/($q.T-$p.T))
            $maxAlpha=[Math]::Min(1.0,($c.T-$p.T)/($q.T-$p.T))
            if ($minAlpha -gt $maxAlpha) { continue }
            $projection=(($c.X-$p.X)*$vx+($c.Y-$p.Y)*$vy+($c.Z-$p.Z)*$vz)/$norm
            $u=[Math]::Clamp([double]$projection,[double]$minAlpha,[double]$maxAlpha)
            $ex=$c.X-($p.X+$u*$vx); $ey=$c.Y-($p.Y+$u*$vy); $ez=$c.Z-($p.Z+$u*$vz)
            $distance=$ex*$ex+$ey*$ey+$ez*$ez
            if ($distance -lt $best-1.e-12) { $best=$distance; $lag=1000*($c.T-($p.T+$u*($q.T-$p.T))) }
        }
        if ($null -eq $lag) { $noHistory++ }
        $rows.Add([pscustomobject]@{Error=$errorCm; Lag=$lag; Residual=if ($null -ne $lag) { [Math]::Sqrt($best) } else { $null }; Bracket=1000*($b.T-$a.T)})
    }
    [pscustomobject]@{Rows=$rows.ToArray(); Outside=$outside; Boundary=$boundary; Mismatch=$mismatch; NoHistory=$noHistory}
}

function Read-PositionTrace([string]$Path, [int]$Authority) {
    $raw=Get-Content -LiteralPath $Path -Raw
    $end=[regex]::Match($raw,'BADMINTON_\w+_TEST (?:PASS|FAIL)[^\r\n]*')
    if ($end.Success) { $raw=$raw.Substring(0,$end.Index+$end.Length) }
    $meta=@([regex]::Matches($raw,'BADMINTON_TRACE_BEGIN run=([a-fA-F0-9]{32}) clock=(\w+) machine=([^\s]+)(?: model=(\w+))?'))
    if ($meta.Count -ne 1) { throw "단일 위치 추적 시작 마커 필요: $Path" }
    $number='(-?\d+(?:\.\d+)?)'
    $pattern="BADMINTON_TRACE_POS authority=$Authority t="+$number+' rally=(-?\d+) sequence=(\d+) flying=([01]) segment=(\d+) x='+$number+' y='+$number+' z='+$number
    $samples=@([regex]::Matches($raw,$pattern) | ForEach-Object {
        New-Position ([double]::Parse($_.Groups[1].Value,$taskCulture)) ([double]::Parse($_.Groups[6].Value,$taskCulture)) ([double]::Parse($_.Groups[7].Value,$taskCulture)) ([double]::Parse($_.Groups[8].Value,$taskCulture)) $_.Groups[3].Value $_.Groups[5].Value $_.Groups[4].Value $_.Groups[2].Value
    })
    if ($samples.Count -lt 2) { throw "위치 표본 부족: $Path" }
    for ($i=1; $i -lt $samples.Count; $i++) { if ($samples[$i].T -le $samples[$i-1].T) { throw "시각 중복/역전: $Path" } }
    [pscustomobject]@{Run=$meta[0].Groups[1].Value; Clock=$meta[0].Groups[2].Value; Machine=$meta[0].Groups[3].Value; Model=if ($meta[0].Groups[4].Success) {$meta[0].Groups[4].Value} else {'Legacy'}; Samples=$samples; Failed=($raw -match 'BADMINTON_\w+_TEST FAIL'); Passed=($raw -match 'BADMINTON_\w+_TEST PASS')}
}

function Assert-TracePair($HostTrace, $ClientTrace) {
    if ($HostTrace.Run -ne $ClientTrace.Run -or $HostTrace.Clock -ne 'windows_qpc' -or $ClientTrace.Clock -ne 'windows_qpc' -or $HostTrace.Machine -ne $ClientTrace.Machine -or $HostTrace.Model -ne $ClientTrace.Model) { throw '실행 GUID·Windows QPC·머신 이름·표시 모델이 일치해야 합니다.' }
}

function Get-PositionStats([object[]]$Values) {
    $v=@($Values | Where-Object {$null -ne $_} | Sort-Object)
    if (-not $v.Count) { return @(0,'미측정','미측정','미측정') }
    $i=[int][Math]::Floor($v.Count/2)
    $median=if ($v.Count%2) {$v[$i]} else {($v[$i-1]+$v[$i])/2}
    return @($v.Count,([double]$median).ToString('0.000',$taskCulture),([double]$v[[int][Math]::Ceiling(.95*$v.Count)-1]).ToString('0.000',$taskCulture),([double]$v[-1]).ToString('0.000',$taskCulture))
}

if ($SelfTest) {
    $server=@(0..50 | ForEach-Object { New-Position ($_*.02) ($_*2) })
    $r=Get-PositionComparison $server @((New-Position .51 41 3))
    if ($r.Rows.Count -ne 1 -or [Math]::Abs($r.Rows[0].Error-[Math]::Sqrt(109)) -gt .00001 -or [Math]::Abs($r.Rows[0].Lag-100) -gt .00001 -or [Math]::Abs($r.Rows[0].Residual-3) -gt .00001) { throw '100ms 지연·3cm 횡방향 오차 분리 실패' }
    $r=Get-PositionComparison $server @((New-Position .51 51))
    if ($r.Rows[0].Error -gt .00001 -or $r.Rows[0].Lag -gt .00001) { throw '동시 좌표 무오차 검사 실패' }
    $r=Get-PositionComparison $server @((New-Position .51 0))
    if ([Math]::Abs($r.Rows[0].Lag-300) -gt .00001) { throw '과거 탐색 상한 실패' }
    $r=Get-PositionComparison $server @((New-Position .51 41 0 0 2),(New-Position 2 10))
    if ($r.Mismatch -ne 1 -or $r.Outside -ne 1 -or $r.Rows.Count) { throw '샷 불일치·시간 범위 제외 실패' }
    $r=Get-PositionComparison @((New-Position 0 0),(New-Position .1 10)) @((New-Position .05 5))
    if ($r.Boundary -ne 1) { throw '긴 프레임 제외 실패' }
    $r=Get-PositionComparison @((New-Position 0 0),(New-Position .02 2 0 0 1 2)) @((New-Position .01 1))
    if ($r.Boundary -ne 1) { throw '충돌 경계 제외 실패' }
    $r=Get-PositionComparison @((New-Position 0 0),(New-Position .02 0)) @((New-Position .01 0))
    if ($r.NoHistory -ne 1 -or $r.Rows[0].Error -ne 0 -or $null -ne $r.Rows[0].Lag) { throw '정지 궤적의 시차 미측정 처리 실패' }
    $shift=@($server | ForEach-Object { New-Position ($_.T+16777216) $_.X })
    $r=Get-PositionComparison $shift @((New-Position (16777216+.51) 41 3))
    if ([Math]::Abs($r.Rows[0].Lag-100) -gt .001) { throw 'QPC 큰 시각 원점 검사 실패' }
    $rejected=$false
    try { Assert-TracePair ([pscustomobject]@{Run='a';Clock='windows_qpc';Machine='same'}) ([pscustomobject]@{Run='b';Clock='windows_qpc';Machine='same'}) } catch { $rejected=$true }
    if (-not $rejected) { throw '다른 실행 로그 차단 실패' }
    Write-Output 'BADMINTON_POSITION_SELFTEST PASS: 동시 위치, 알려진 시차/횡오차, 탐색 상한, 상태/충돌/시간 경계, 정지, 큰 시각 원점, 다른 실행 차단'
    return
}

if (-not $SameMachine) { throw '동일 Windows PC에서 같은 실행으로 수집한 로그에만 -SameMachine을 지정하세요.' }
if (-not $HostLog -or -not $ClientLog) { throw 'HostLog와 ClientLog가 필요합니다.' }
$taskHostPath=(Resolve-Path -LiteralPath $HostLog).Path
$taskClientPath=(Resolve-Path -LiteralPath $ClientLog).Path
if ($taskHostPath -eq $taskClientPath) { throw '서로 다른 호스트·참가자 로그가 필요합니다.' }
$taskHostTrace=Read-PositionTrace $taskHostPath 1
$taskClientTrace=Read-PositionTrace $taskClientPath 0
Assert-TracePair $taskHostTrace $taskClientTrace
$taskResult=Get-PositionComparison $taskHostTrace.Samples $taskClientTrace.Samples
if ($taskResult.Rows.Count -lt 30) { throw '유효한 동일 시점 비교 표본이 30개 미만입니다.' }
$taskDate=[TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTime]::UtcNow,'Korea Standard Time').ToString('yy.MM.dd')
$taskFolder=Join-Path (Split-Path -Parent $PSScriptRoot) "Docs/$taskDate"
New-Item -ItemType Directory -Path $taskFolder -Force | Out-Null
$taskRun=[IO.Path]::GetFileNameWithoutExtension($taskHostPath) -replace '[^A-Za-z0-9_-]','_'
$taskReport=Join-Path $taskFolder "$taskDate - 동시 위치 비교 $taskRun - GPT-6.md"
$taskLines=[Collections.Generic.List[string]]::new()
$taskLines.Add('# 공통 시각 기준 셔틀 위치 비교')
$taskLines.Add('')
$taskLines.Add("작성일: $taskDate · 모델: GPT-6 · 편도 설정 $OneWayLagMs ms · 송신 손실 $PacketLossPercent %")
$taskLines.Add('')
$taskLines.Add("실행 GUID: $($taskHostTrace.Run). 동일 PC 선언 및 두 로그의 GUID·머신·Windows QPC 메타데이터 일치 확인.")
$taskLines.Add("표시 모델: $($taskHostTrace.Model). RoundTrip은 왕복 측정 기반 표시 시계, Legacy는 기존 GameState 추정과 수신 시간 하한이다. RoundTrip도 미확정/만료 구간에는 기존 방식으로 돌아간다.")
$taskLines.Add("검사 마커: 호스트 PASS=$($taskHostTrace.Passed), FAIL=$($taskHostTrace.Failed); 참가자 PASS=$($taskClientTrace.Passed), FAIL=$($taskClientTrace.Failed). 보고서 생성만으로 게임 검사 성공을 뜻하지 않는다.")
$taskLines.Add('')
$taskLines.Add("호스트 로그: ``$taskHostPath``")
$taskLines.Add("참가자 로그: ``$taskClientPath``")
$taskLines.Add('')
$taskLines.Add('| 지표 | 표본 | 중앙값 | p95 | 최댓값 |')
$taskLines.Add('|---|---:|---:|---:|---:|')
foreach ($metric in @(@('동시 서버 기준 위치 차이(cm)','Error'),@('과거 궤적 최근접 시차(ms)','Lag'),@('최근접 과거 궤적 잔여 거리(cm)','Residual'),@('서버 보간 구간 길이(ms)','Bracket'))) {
    $stats=Get-PositionStats @($taskResult.Rows | ForEach-Object {$_.PSObject.Properties[$metric[1]].Value})
    $taskLines.Add("| $($metric[0]) | $($stats -join ' | ') |")
}
$taskFlying=@($taskClientTrace.Samples | Where-Object {$_.Flying -eq 1}).Count
$taskCapped=@($taskResult.Rows | Where-Object {$null -ne $_.Lag -and $_.Lag -ge 299.999}).Count
$taskLines.Add('')
$taskLines.Add("참가자 비행 표본 $taskFlying, 비교 유효 $($taskResult.Rows.Count), 서버 시각 범위 밖 $($taskResult.Outside), 전환/충돌/긴 프레임 경계 $($taskResult.Boundary), 샷·랠리 불일치 $($taskResult.Mismatch). 유효 표본 중 움직이는 과거 궤적 없음 $($taskResult.NoHistory), 300ms 탐색 끝 선택 $taskCapped.")
$taskLines.Add('')
$taskLines.Add('## 해석 범위')
$taskLines.Add('')
$taskLines.Add('- 같은 PC에서 QPC 시각으로 기록한 게임 스레드 Tick 종료 위치를 비교한다. 실제 디스플레이 출력·GPU·입력→화면 지연을 측정하지 않는다. 별도 PC·외부망 로그에는 사용할 수 없다.')
$taskLines.Add('- 서버 기준은 전후 표본의 선형 보간 근사다. 최대 50ms 구간만 사용하며 이산 시뮬레이션·프레임 위상·가속도 오차가 포함된다. 연속 궤적의 정확한 실제 좌표라고 주장하지 않는다.')
$taskLines.Add('- 샷·랠리·비행 상태 전환 및 서버 충돌 세그먼트 경계는 보간하지 않는다. 참가자의 샷/랠리가 현재 서버와 다르면 별도 제외한다. 이 제외 표본에도 사용자가 보는 전환 지연이 있을 수 있다.')
$taskLines.Add('- 과거 300ms의 같은 샷·같은 충돌 세그먼트에서 가장 가까운 지점을 찾는다. 시차는 궤적 추종의 기하학적 추정이며 네트워크 RTT/편도 전송 시간을 의미하지 않는다. 정지 궤적은 시차를 산출하지 않는다.')
$taskLines.Add('- 각 프로세스 첫 TEST PASS/FAIL까지로 고정한다. 수동 로그에 마커가 없으면 읽은 전체 구간이다. 중앙값은 홀짝 중앙값, p95는 nearest-rank다. 표본을 독립된 반복 실험으로 간주하지 않는다.')
$taskLines.Add('- 추적은 선택적 개발 로그이며 네트워크 패킷을 추가하지 않는다. 프레임별 로그 비용은 남으므로 추적을 끈 실제 플레이 성능과 동일하다고 가정하지 않는다.')
$taskLines.Add('- 시각 근거: [Microsoft QPC 설명](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps).')
[IO.File]::WriteAllLines($taskReport,$taskLines,[Text.UTF8Encoding]::new($false))
Write-Output "BADMINTON_POSITION_REPORT: $taskReport"
Write-Output "동시 위치 비교=$($taskResult.Rows.Count), 전환 경계=$($taskResult.Boundary), 샷/랠리 불일치=$($taskResult.Mismatch)"
