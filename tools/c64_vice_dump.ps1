# Warp-boot MM2 in VICE and save RAM $0800-$FFFF via remote monitor (127.0.0.1:6510).
param(
  [int] $Disk = 1,
  [int] $Seconds = 90,
  [string] $OutName = '',
  [switch] $TrueDrive
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path $PSScriptRoot -Parent
$D64 = Join-Path $RepoRoot "EXTRACTED\c64\MM2-$Disk.D64"
$Emu = Join-Path $RepoRoot 'EXTRACTED\c64\emulator'
$RamDir = Join-Path $Emu 'ram'
$LogDir = Join-Path $Emu 'logs'
$Scripts = Join-Path $Emu 'scripts'
New-Item -ItemType Directory -Force -Path $RamDir, $LogDir, $Scripts | Out-Null

if (-not $OutName) { $OutName = "mm2-${Disk}_boot_${Seconds}s.bin" }
$RamOut = (Join-Path $RamDir $OutName).Replace('\', '/')

$ViceDir = $env:VICE_ROOT
if (-not $ViceDir) {
  $cmd = Get-Command x64sc.exe -ErrorAction SilentlyContinue
  if ($cmd) { $ViceDir = Split-Path $cmd.Source -Parent }
}
if (-not $ViceDir) { throw 'x64sc not found; install VICE via winget or set $env:VICE_ROOT.' }
$X64 = Join-Path $ViceDir 'x64sc.exe'
$RemotePy = Join-Path $Scripts 'vice_remote_save.py'

$RunMon = Join-Path $Scripts 'run_only.mon'
'x' | Set-Content -Path $RunMon -Encoding ASCII

$Shot = Join-Path $LogDir "mm2-${Disk}_dump_exit.png"
$Args = @(
  '-autostart', $D64,
  '-autostart-warp', '-autostart-delay', '1', '-autostart-handle-tde',
  '-drive8type', '1541',
  '+sound', '-warp',
  '-moncommands', $RunMon,
  '-remotemonitor', '-remotemonitoraddress', '127.0.0.1:6510',
  '+confirmonexit',
  '-exitscreenshot', $Shot,
  '-jamaction', '1'
)
if ($TrueDrive) { $Args += '-drive8truedrive' }

Write-Host "Booting $D64 for $Seconds s (warp)..."
$Proc = Start-Process -FilePath $X64 -WorkingDirectory $ViceDir -PassThru -WindowStyle Minimized -ArgumentList $Args
Start-Sleep -Seconds $Seconds
python $RemotePy $RamOut
Start-Sleep -Seconds 2
if (-not $Proc.HasExited) { Stop-Process -Id $Proc.Id -Force -ErrorAction SilentlyContinue }

$LocalRam = Join-Path $RamDir $OutName
if (Test-Path $LocalRam) {
  Write-Host "Saved $LocalRam ($((Get-Item $LocalRam).Length) bytes)"
} else {
  Write-Warning 'RAM dump missing — check monitor connection / vice_remote_save.py output.'
}
