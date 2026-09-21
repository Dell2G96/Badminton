param([switch]$SelfTest)

$ErrorActionPreference = 'Stop'

function Protect-EOSSettings([string]$Json) {
	$secure = ConvertTo-SecureString -String $Json -AsPlainText -Force
	try { return ConvertFrom-SecureString -SecureString $secure }
	finally { $secure.Dispose() }
}

function Unprotect-EOSSettings([string]$Ciphertext) {
	$secure = ConvertTo-SecureString -String $Ciphertext
	$pointer = [IntPtr]::Zero
	try {
		$pointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
		return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($pointer)
	} finally {
		if ($pointer -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($pointer) }
		$secure.Dispose()
	}
}

if ($SelfTest) {
	$sample = '{"SchemaVersion":1,"ClientSecret":"synthetic-test-value"}'
	$cipher = Protect-EOSSettings $sample
	if ($cipher.Contains('synthetic-test-value')) { throw 'Plaintext found in protected output.' }
	if ((Unprotect-EOSSettings $cipher) -cne $sample) { throw 'Windows encryption round-trip failed.' }
	Write-Output 'PASS: Windows DPAPI round-trip with synthetic data; no credential file written.'
	return
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$projectRoot = Split-Path -Parent $PSScriptRoot
$secretDirectory = Join-Path $projectRoot 'LocalSecrets'
$destination = Join-Path $secretDirectory 'EOS.credentials.dpapi'
$form = New-Object System.Windows.Forms.Form
$form.Text = '배드민턴 EOS - 이 PC에 연결 정보 보관'
$form.ClientSize = New-Object System.Drawing.Size(680,450)
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.Font = New-Object System.Drawing.Font('Malgun Gothic',10)

$intro = New-Object System.Windows.Forms.Label
$intro.Text = "포털의 복사 버튼으로 아래 5개 값을 입력하세요. 애플리케이션 ID는 필요 없습니다.`r`n값은 이 Windows 사용자 계정으로 암호화해 보관합니다. 아직 EOS에 접속하지 않습니다."
$intro.SetBounds(20,15,640,55)
$form.Controls.Add($intro)

$fields = @{}
$names = @('ProductId','SandboxId','DeploymentId','ClientId','ClientSecret')
$labels = @('제품 ID','샌드박스 ID (Live)','디플로이 ID (Live Deployment)','클라이언트 ID','클라이언트 비밀 키')
for ($i = 0; $i -lt $names.Count; $i++) {
	$label = New-Object System.Windows.Forms.Label
	$label.Text = $labels[$i]
	$label.SetBounds(20,82 + 48*$i,225,28)
	$form.Controls.Add($label)
	$box = New-Object System.Windows.Forms.TextBox
	$box.SetBounds(250,79 + 48*$i,410,28)
	$box.MaxLength = 1024
	$box.UseSystemPasswordChar = $names[$i] -eq 'ClientSecret'
	$form.Controls.Add($box)
	$fields[$names[$i]] = $box
}

$identifierPath = Join-Path $secretDirectory 'EOS.identifiers.json'
if (Test-Path -LiteralPath $identifierPath) {
	$identifiers = Get-Content -LiteralPath $identifierPath -Raw | ConvertFrom-Json
	foreach ($name in @('ProductId','SandboxId','DeploymentId','ClientId')) {
		if ($identifiers.$name) { $fields[$name].Text = [string]$identifiers.$name }
	}
}

$status = New-Object System.Windows.Forms.Label
$status.Text = '이 파일은 다른 PC/Windows 계정으로 복사해도 그대로 사용할 수 없습니다.'
$status.SetBounds(20,330,640,55)
$form.Controls.Add($status)

$save = New-Object System.Windows.Forms.Button
$save.Text = '암호화하여 보관'
$save.SetBounds(440,393,155,36)
$form.Controls.Add($save)
$cancel = New-Object System.Windows.Forms.Button
$cancel.Text = '닫기'
$cancel.SetBounds(603,393,57,36)
$cancel.DialogResult = [System.Windows.Forms.DialogResult]::Cancel
$form.Controls.Add($cancel)
$form.CancelButton = $cancel
$form.AcceptButton = $save

$save.Add_Click({
	try {
		$payload = [ordered]@{SchemaVersion=1; ArtifactName='BadmintonDev'; AuthScopeFlags=@('BasicProfile')}
		foreach ($name in $names) {
			$value = $fields[$name].Text.Trim()
			if ([string]::IsNullOrWhiteSpace($value) -or $value -match '[\r\n]') {
				$status.Text = '모든 항목에 포털에서 복사한 한 줄 값을 입력하세요.'
				$fields[$name].Focus()
				return
			}
			if ($name -in @('ProductId','DeploymentId') -and $value -notmatch '^[0-9a-fA-F]{32}$') {
				$status.Text = '제품 ID와 디플로이 ID는 포털의 32자리 값을 확인하세요.'
				$fields[$name].Focus()
				return
			}
			$payload[$name] = $value
		}
		if (Test-Path -LiteralPath $destination) {
			$answer = [System.Windows.Forms.MessageBox]::Show($form,'기존에 보관한 EOS 연결 정보를 새 입력값으로 교체할까요?','기존 정보 교체','YesNo','Question')
			if ($answer -ne [System.Windows.Forms.DialogResult]::Yes) { return }
		}
		$ciphertext = Protect-EOSSettings ($payload | ConvertTo-Json -Compress)
		[System.IO.Directory]::CreateDirectory($secretDirectory) | Out-Null
		[System.IO.File]::WriteAllText($destination,$ciphertext,[System.Text.Encoding]::ASCII)
		$payload = $null
		$value = $null
		foreach ($box in $fields.Values) { $box.Clear() }
		$status.Text = "보관 완료. 이 창을 닫고 채팅에 '로컬 보관 완료'라고 알려주세요."
		$save.Enabled = $false
	} catch {
		$status.Text = '보관하지 못했습니다. 입력값은 공유하지 말고 폴더 쓰기 권한을 확인하세요.'
	}
})

try { [void]$form.ShowDialog() }
finally { foreach ($box in $fields.Values) { $box.Clear() }; $form.Dispose() }
