param([string]$EngineRoot = 'D:\Game\UE_5.7', [switch]$ResumeStaging, [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$PackageName = 'BadmintonMVP')

$ErrorActionPreference = 'Stop'
$taskProjectRoot = Split-Path -Parent $PSScriptRoot
$taskRun = Get-Date -Format 'yyyyMMdd-HHmmss'
$taskLog = Join-Path $taskProjectRoot "Saved\Logs\BadmintonPackage-$taskRun.log"
$taskPreviousLog = $env:uebp_LogFolder
$taskPreviousFinalLog = $env:uebp_FinalLogFolder
$taskPreviousSaved = $env:uebp_EngineSavedFolder
try {
    # UAT clears its log folder on startup, so allocate a new dedicated directory.
    $env:uebp_LogFolder = Join-Path $taskProjectRoot "Saved\Logs\UAT-Badminton-$taskRun"
    $env:uebp_FinalLogFolder = $env:uebp_LogFolder
    $env:uebp_EngineSavedFolder = Join-Path $taskProjectRoot "Saved\UAT-Badminton-$taskRun"
    New-Item -ItemType Directory -Path $env:uebp_LogFolder -Force | Out-Null
    Write-Output "패키징 로그: $taskLog"
    [string[]]$taskBuildCook = if ($ResumeStaging) { @('-skipcook') } else { @('-build','-cook') }
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat') BuildCookRun "-project=$taskProjectRoot\FPS_Dell2g.uproject" `
        -noP4 -platform=Win64 -clientconfig=Development @taskBuildCook '-map=/Game/Badminton/Maps/L_Badminton_Prototype' `
        -stage -pak -archive "-archivedirectory=$taskProjectRoot\Saved\Packages\$PackageName" -unattended -utf8output `
        '-ubtargs=-NoUBA -MaxParallelActions=6' "-AdditionalCookerOptions=-ddc=NoZenLocalFallback -LocalDataCachePath=$taskProjectRoot\DerivedDataCache" *> $taskLog
    if ($LASTEXITCODE -ne 0) { throw "패키징 실패: $taskLog" }
    Write-Output "패키징 완료: $taskProjectRoot\Saved\Packages\$PackageName"
} finally {
    $env:uebp_LogFolder = $taskPreviousLog
    $env:uebp_FinalLogFolder = $taskPreviousFinalLog
    $env:uebp_EngineSavedFolder = $taskPreviousSaved
}
