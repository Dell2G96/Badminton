param(
    [ValidateSet('Editor', 'Solo', 'Host', 'Join')]
    [string]$Mode = 'Editor',
    [string]$Address = '127.0.0.1:7777',
    [string]$EngineRoot = 'D:\Game\UE_5.7'
)

$ErrorActionPreference = 'Stop'
$taskProjectRoot = Split-Path -Parent $PSScriptRoot
$taskEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$taskMap = '/Game/Badminton/Maps/L_Badminton_Prototype'
if ($Mode -eq 'Host') { $taskMap += '?listen' }
if ($Mode -eq 'Join') { $taskMap = $Address }
$taskArguments = @('"' + (Join-Path $taskProjectRoot 'FPS_Dell2g.uproject') + '"', $taskMap,
    '-ddc=NoZenLocalFallback', '"-LocalDataCachePath=' + $taskProjectRoot + '\DerivedDataCache"')
if ($Mode -ne 'Editor') { $taskArguments += @('-game', '-windowed', '-ResX=1280', '-ResY=720') }
# This launcher is for the user to open an interactive editor or game window.
Start-Process -FilePath $taskEditor -ArgumentList $taskArguments
