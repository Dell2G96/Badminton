param([string]$GameExecutable, [int]$BasePort = 20200)

$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskDate = Get-Date -Format 'yy.MM.dd'
$taskReportRoot = Join-Path $taskRoot "Docs\$taskDate"
New-Item -ItemType Directory -Path $taskReportRoot -Force | Out-Null
$taskReport = Join-Path $taskReportRoot "$taskDate - 배드민턴 랠리 네트워크 조합 검증 $(Get-Date -Format 'HHmmss') - GPT-6.md"
@'
# 배드민턴 랠리 네트워크 조합 검증

모델: GPT-6. 각 조건에서 별도 호스트·참가자 프로세스를 실행한다. 같은 랠리에서 교대로 10회 타격해야 통과한다. 명목 RTT는 편도 설정값의 두 배이며 실제 계측 RTT가 아니다.

| 편도 지연 ms | 명목 RTT ms | 송신 손실 % | 결과 |
|---|---|---|---|
'@ | Set-Content -LiteralPath $taskReport -Encoding utf8
$taskIndex = 0
$taskFailures = 0
$taskPowerShell = (Get-Process -Id $PID).Path
foreach ($taskLag in @(0,25,50,75)) {
    foreach ($taskLoss in @(0,1,3)) {
        $taskArgs = @('-NoProfile','-File',(Join-Path $PSScriptRoot 'Test-BadmintonNetwork.ps1'),'-TestKind','Rally',
            '-Port',($BasePort + $taskIndex),'-OneWayLagMs',$taskLag,'-PacketLossPercent',$taskLoss)
        if ($GameExecutable) { $taskArgs += @('-GameExecutable',$GameExecutable) }
        Write-Output "검증 $($taskIndex+1)/12: 편도=$taskLag ms, 손실=$taskLoss %"
        $taskOutput = & $taskPowerShell @taskArgs 2>&1 | Out-String
        $taskPassed = $LASTEXITCODE -eq 0 -and ([regex]::Matches($taskOutput,'BADMINTON_SHOT_TEST PASS').Count -eq 2)
        $taskResult = if ($taskPassed) {'통과'} else {$taskFailures++; '실패'}
        "| $taskLag | $($taskLag*2) | $taskLoss | $taskResult |" | Add-Content -LiteralPath $taskReport -Encoding utf8
        $taskDetails = Join-Path $taskRoot "Saved\Logs\BadmintonMatrix-$taskDate-$($BasePort+$taskIndex).log"
        $taskOutput | Set-Content -LiteralPath $taskDetails -Encoding utf8
        Write-Output "$taskResult - $taskDetails"
        $taskIndex++
    }
}
"`n총 $taskIndex 조건, 실패 $taskFailures 건. 세부 실행 출력은 Saved/Logs/BadmintonMatrix-$taskDate-포트.log에 보관한다." | Add-Content -LiteralPath $taskReport -Encoding utf8
Write-Output "결과 문서: $taskReport"
if ($taskFailures) { throw '일부 네트워크 조합에서 실패했습니다. 결과 문서를 확인하세요.' }
