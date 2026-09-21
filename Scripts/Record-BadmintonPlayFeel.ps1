param([int]$Seconds=40)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskRun=Get-Date -Format 'yyyyMMdd-HHmmss'
$taskLog=Join-Path $taskRoot "Saved\Logs\PlayFeelRecording-$taskRun.log"
$taskVideo=Join-Path $taskRoot "Docs\_녹화파일\26.09.15 - 조작감 개선 실제 플레이 $taskRun - GPT-6.mp4"
New-Item -ItemType Directory -Path (Split-Path -Parent $taskVideo) -Force | Out-Null
$taskExe=Join-Path $taskRoot 'Saved\Packages\BadmintonFPS\Windows\FPS_Dell2g\Binaries\Win64\FPS_Dell2g.exe'
$taskGame=$null
$taskRecorder=$null
try {
 $taskGame=Start-Process -FilePath $taskExe -ArgumentList @('/Game/Badminton/Maps/L_Badminton_Prototype','-BadmintonTimingTest','-windowed','-ResX=1280','-ResY=720','-WinX=40','-WinY=40','-unattended','-nosplash','-ExecCmds="t.MaxFPS 60"',('-abslog="'+$taskLog+'"')) -PassThru -WindowStyle Normal
 $taskDeadline=(Get-Date).AddSeconds(20)
 do { Start-Sleep -Milliseconds 250; $taskGame.Refresh() } while ($taskGame.MainWindowHandle -eq 0 -and -not $taskGame.HasExited -and (Get-Date) -lt $taskDeadline)
 if ($taskGame.HasExited -or $taskGame.MainWindowHandle -eq 0) { throw 'Game window unavailable for recording' }
 $taskTitle=$taskGame.MainWindowTitle
 Write-Output "GAME PID=$($taskGame.Id) WINDOW=$taskTitle LOG=$taskLog"
 $taskRecorder=Start-Process -FilePath 'F:\_Recording\ffmpeg-2025-01-22-git-e20ee9f9ae-full_build\bin\ffmpeg.exe' -ArgumentList @('-hide_banner','-loglevel','warning','-f','gdigrab','-framerate','30','-draw_mouse','0','-i',('"title='+$taskTitle+'"'),'-t',"$Seconds",'-an','-c:v','libx264','-preset','veryfast','-crf','22','-pix_fmt','yuv420p','-vf','pad=ceil(iw/2)*2:ceil(ih/2)*2','-movflags','+faststart',('"'+$taskVideo+'"')) -PassThru -WindowStyle Hidden -RedirectStandardError (Join-Path $taskRoot "Saved\Logs\PlayFeelFFmpeg-$taskRun.log")
 $taskDeadline=(Get-Date).AddSeconds($Seconds+30)
 do {
  Start-Sleep -Milliseconds 500
  $taskRecorder.Refresh()
  if ((Get-Date) -gt $taskDeadline) { throw 'Recording timeout' }
 } while (-not $taskRecorder.HasExited)
 if ($taskRecorder.ExitCode -ne 0) { throw 'FFmpeg recording failed' }
 $taskBlackLog=Join-Path $taskRoot "Saved\Logs\PlayFeelBlackCheck-$taskRun.log"
 & 'F:\_Recording\ffmpeg-2025-01-22-git-e20ee9f9ae-full_build\bin\ffmpeg.exe' -hide_banner -i $taskVideo -vf 'blackdetect=d=5:pix_th=0.10' -an -f null NUL 2> $taskBlackLog
 if ($LASTEXITCODE -ne 0) { throw 'Recorded video cannot be decoded' }
 $taskBlackText=Get-Content -LiteralPath $taskBlackLog -Raw
 foreach ($taskBlack in [regex]::Matches($taskBlackText,'black_duration:([0-9.]+)')) {
  if ([double]$taskBlack.Groups[1].Value -gt $Seconds * .9) { throw 'Recording is black; do not present as a successful demo' }
 }
 $taskText=Get-Content -LiteralPath $taskLog -Raw
 if ($taskText -match 'BADMINTON_TIMING_TEST FAIL[^\r\n]*') { throw $Matches[0] }
 if ($taskText -notmatch 'BADMINTON_TIMING_TEST PASS[^\r\n]*') { throw 'Integrated game test not completed within recording' }
 Write-Output $Matches[0]
 Write-Output "VIDEO=$taskVideo"
} finally {
 if ($taskRecorder -and -not $taskRecorder.HasExited) { Stop-Process -Id $taskRecorder.Id -Force }
 if ($taskGame -and -not $taskGame.HasExited) { Stop-Process -Id $taskGame.Id -Force }
}


