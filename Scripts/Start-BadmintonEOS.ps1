param(
	[string]$EngineRoot = 'D:\Game\UE_5.7',
	[string]$GameExecutable,
	[switch]$ConfigurationTest
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if ($GameExecutable) { $editor = (Resolve-Path -LiteralPath $GameExecutable).Path }
if (-not (Test-Path -LiteralPath $editor)) { throw '게임 실행 파일을 찾을 수 없습니다.' }
$secretRoot = Join-Path $projectRoot 'LocalSecrets'
$credentialFile = Join-Path $secretRoot 'EOS.credentials.dpapi'

if ($ConfigurationTest) {
	# Synthetic values only; the integration test verifies config loading, not authentication.
	$settings = [pscustomobject]@{
		ProductId=('1' * 32); SandboxId=('2' * 32); DeploymentId=('3' * 32)
		ClientId='xyzBadmintonSyntheticClient'; ClientSecret='SyntheticOnlyNotARealSecret'
	}
} else {
	if (-not (Test-Path -LiteralPath $credentialFile)) { throw '먼저 Open-EOS-Setup.cmd로 온라인 연결을 설정하세요.' }
	$secure = $null
	$pointer = [IntPtr]::Zero
	try {
		$secure = ConvertTo-SecureString ([System.IO.File]::ReadAllText($credentialFile))
		$pointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
		$settings = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($pointer) | ConvertFrom-Json
	} catch {
		throw '저장된 온라인 설정을 읽을 수 없습니다. 설정을 저장한 Windows 계정으로 이 실행 메뉴를 직접 실행하세요.'
	} finally {
		if ($pointer -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($pointer) }
		if ($secure) { $secure.Dispose() }
	}
}

foreach ($field in @('ProductId','SandboxId','DeploymentId','ClientId','ClientSecret')) {
	$value = [string]$settings.$field
	if ([string]::IsNullOrWhiteSpace($value) -or $value -match '[\s"(),\\=]') {
		throw '온라인 설정에 빈 값이나 사용할 수 없는 문자가 있습니다. 로컬 설정 입력을 확인하세요.'
	}
}

$runtimeDirectory = Join-Path $secretRoot ('Run-' + [Guid]::NewGuid().ToString('N'))
$runtimeIni = Join-Path $runtimeDirectory 'Engine.ini'
$gameProcess = $null
try {
	[System.IO.Directory]::CreateDirectory($runtimeDirectory) | Out-Null
	$acl = New-Object System.Security.AccessControl.DirectorySecurity
	$acl.SetAccessRuleProtection($true,$false)
	$currentSid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User
	$rule = New-Object System.Security.AccessControl.FileSystemAccessRule($currentSid,'FullControl','ContainerInherit,ObjectInherit','None','Allow')
	$acl.AddAccessRule($rule)
	Set-Acl -LiteralPath $runtimeDirectory -AclObject $acl
	# -EngineIni points the generated config layer at this per-run file.
	# No secret is placed in Default*.ini, command-line arguments, or documentation.
	$ini = @"
[OnlineSubsystem]
DefaultPlatformService=EOS

[OnlineSubsystemEOS]
bEnabled=true

[/Script/Engine.Engine]
NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SocketSubsystemEOS.NetDriverEOSBase",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
NetDriverDefinitions=(DefName="DemoNetDriver",DriverClassName="/Script/Engine.DemoNetDriver",DriverClassNameFallback="/Script/Engine.DemoNetDriver")

[/Script/SocketSubsystemEOS.NetDriverEOSBase]
bIsUsingP2PSockets=true

[/Script/OnlineSubsystemEOS.EOSSettings]
DefaultArtifactName=BadmintonDev
CacheDir=CacheDir
bUseEAS=true
bUseEOSConnect=true
bEnableOverlay=false
bEnableSocialOverlay=false
bEnableEditorOverlay=false
bUseEOSSessions=true
bMirrorStatsToEOS=false
bMirrorAchievementsToEOS=false
bMirrorPresenceToEAS=false
AuthScopeFlags=BasicProfile
Artifacts=(ArtifactName="BadmintonDev",ClientId="$($settings.ClientId)",ClientSecret="$($settings.ClientSecret)",ProductId="$($settings.ProductId)",SandboxId="$($settings.SandboxId)",DeploymentId="$($settings.DeploymentId)",ClientEncryptionKey="$('1' * 64)")

[Core.Log]
LogEOSSDK=Error
LogOnline=Warning
LogOnlineIdentity=Warning
"@
	[System.IO.File]::WriteAllText($runtimeIni,$ini,(New-Object System.Text.UTF8Encoding($false)))
	$ini = $null
	$settings = $null
	$value = $null
	$arguments = @(
		('"' + (Join-Path $projectRoot 'FPS_Dell2g.uproject') + '"'),
		'/Game/Badminton/Maps/L_Badminton_Prototype', '-game', '-BadmintonEOS',
		('-EngineIni="' + $runtimeIni + '"'), '-NOAUTOINIUPDATE',
		'-ddc=NoZenLocalFallback', ('-LocalDataCachePath="' + (Join-Path $projectRoot 'DerivedDataCache') + '"')
	)
	if ($GameExecutable) { $arguments = $arguments[1..($arguments.Count - 1)] }
	if ($ConfigurationTest) {
		$arguments += @('-NullRHI','-unattended','-nosound','-BadmintonEOSConfigurationTest',
			'-ExecCmds="Automation RunTests FPS_Dell2g.EOS.Configuration;Quit"',
			('-abslog="' + (Join-Path $projectRoot 'Saved\Logs\BadmintonEOSConfigTest.log') + '"'))
		$gameProcess = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
		if (-not $gameProcess.WaitForExit(120000)) {
			Stop-Process -Id $gameProcess.Id
			$gameProcess.WaitForExit()
			throw '설정 검사 시간이 초과되었습니다.'
		}
	} else {
		$arguments += @('-windowed','-ResX=1280','-ResY=720')
		$gameProcess = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Normal -PassThru
		Write-Host '온라인 게임을 실행했습니다. 게임이 끝날 때까지 이 창을 열어두세요.'
		$gameProcess.WaitForExit()
	}
	if ($gameProcess.ExitCode -ne 0) { throw '게임이 오류로 종료되었습니다. 로컬 게임 로그를 확인하세요.' }
	if ($ConfigurationTest) {
		$testLog = Get-Content -LiteralPath (Join-Path $projectRoot 'Saved\Logs\BadmintonEOSConfigTest.log') -Raw
		if ($testLog -notmatch 'Test Completed\. Result=\{Success\} Name=\{Configuration\} Path=\{FPS_Dell2g\.EOS\.Configuration\}') {
			throw '온라인 설정 검사의 성공 결과를 확인하지 못했습니다.'
		}
		Write-Output 'EOS_CONFIGURATION_TEST PASS (synthetic values only)'
	}
} finally {
	# These are exact files owned by this run. Never delete another run's directory.
	if ($gameProcess -and -not $gameProcess.HasExited) { $gameProcess.WaitForExit() }
	if (Test-Path -LiteralPath $runtimeIni) { Remove-Item -LiteralPath $runtimeIni -Force }
	if ((Test-Path -LiteralPath $runtimeDirectory) -and -not (Get-ChildItem -LiteralPath $runtimeDirectory -Force | Select-Object -First 1)) {
		Remove-Item -LiteralPath $runtimeDirectory
	}
}
