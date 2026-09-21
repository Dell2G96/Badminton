param(
    [ValidateSet('Solo','Host','Join','EOS')][string]$Mode = 'Solo',
    [string]$Address = '127.0.0.1:7777',
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$PackageName = 'BadmintonOnlineLobby',
    [switch]$LegacyFirstPerson
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskExecutable = Join-Path $taskRoot "Saved\Packages\$PackageName\Windows\FPS_Dell2g\Binaries\Win64\FPS_Dell2g.exe"
if (-not (Test-Path -LiteralPath $taskExecutable)) { throw '먼저 Scripts/Package-Badminton.ps1로 패키징하세요.' }
if ($Mode -eq 'EOS') {
    & (Join-Path $PSScriptRoot 'Start-BadmintonEOS.ps1') -GameExecutable $taskExecutable
    exit
}
$taskMap = '/Game/Badminton/Maps/L_Badminton_Prototype'
if ($Mode -eq 'Host') { $taskMap += '?listen' }
if ($Mode -eq 'Join') {
    if ($Address -notmatch '^[a-zA-Z0-9.:-]+$') { throw '주소에는 호스트 이름 또는 IP와 포트만 입력하세요.' }
    $taskMap = $Address
}
$taskArguments = @($taskMap,'-windowed','-ResX=1280','-ResY=720')
if ($LegacyFirstPerson) { $taskArguments += '-BadmintonLegacyFirstPerson' }
Start-Process -FilePath $taskExecutable -ArgumentList $taskArguments -WindowStyle Normal
