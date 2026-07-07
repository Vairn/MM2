# Launch BlastEm for MM2 Genesis VRAM validation (manual VRAM debug or GST save).
param(
  [string] $BlastEmDir = '',
  [string] $Rom = '',
  [switch] $Debugger,
  [int] $Seconds = 0
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path $PSScriptRoot -Parent
if (-not $BlastEmDir) {
  $BlastEmDir = Join-Path $RepoRoot 'tools\emulators\BlastEm\blastem-win32-0.6.2'
}
$BlastExe = Join-Path $BlastEmDir 'blastem.exe'
if (-not $Rom) {
  $Rom = Join-Path $RepoRoot 'EXTRACTED\Genesis\Might and Magic - Gates to Another World (USA, Europe).gen'
}
$EmuDir = Join-Path $RepoRoot 'EXTRACTED\genesis\emulator'
New-Item -ItemType Directory -Force -Path $EmuDir | Out-Null

if (-not (Test-Path $BlastExe)) { throw "blastem.exe not found: $BlastExe (see tools/emulators/README.md)" }
if (-not (Test-Path $Rom)) { throw "ROM not found: $Rom" }

$ShortRom = Join-Path $EmuDir 'mm2.gen'
Copy-Item $Rom $ShortRom -Force

$cfgDir = Join-Path $env:LOCALAPPDATA 'blastem'
New-Item -ItemType Directory -Force -Path $cfgDir | Out-Null
$cfg = Join-Path $cfgDir 'blastem.cfg'
if (-not (Test-Path $cfg)) { Copy-Item (Join-Path $BlastEmDir 'default.cfg') $cfg }

$args = @('-m', 'gen', '-r', 'E')
if ($Debugger) { $args += '-d' }
$args += "`"$ShortRom`""

Write-Host "Starting BlastEm: $BlastExe"
Write-Host "ROM: $ShortRom"
Write-Host ""
Write-Host "VRAM: toggle ui.vram_debug in-game, or debugger (-d) with p/di commands."
Write-Host "Save states: F5 quicksave (.gst in save_path); compare via genesis_vram_dump_stub.py"
Write-Host ""

$proc = Start-Process -FilePath $BlastExe -WorkingDirectory $BlastEmDir -ArgumentList $args -PassThru
if ($Seconds -gt 0) {
  Start-Sleep -Seconds $Seconds
  if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
}
