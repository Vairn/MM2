# Launch Mesen 2 for MM2 SNES VRAM validation (manual Video RAM export required).
param(
  [string] $MesenExe = '',
  [string] $Rom = '',
  [switch] $NoLaunch
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path $PSScriptRoot -Parent
if (-not $MesenExe) {
  $MesenExe = Join-Path $RepoRoot 'tools\emulators\Mesen2\Mesen.exe'
}
if (-not $Rom) {
  $Rom = Join-Path $RepoRoot 'EXTRACTED\SNES\Might and Magic II (Europe).sfc'
}
$EmuDir = Join-Path $RepoRoot 'EXTRACTED\snes\emulator'
$DumpDir = Join-Path $EmuDir 'dumps'
New-Item -ItemType Directory -Force -Path $EmuDir, $DumpDir | Out-Null

if (-not (Test-Path $MesenExe)) { throw "Mesen.exe not found: $MesenExe (see tools/emulators/README.md)" }
if (-not (Test-Path $Rom)) { throw "ROM not found: $Rom" }

$ShortRom = Join-Path $EmuDir 'mm2.sfc'
Copy-Item $Rom $ShortRom -Force

$DumpPath = Join-Path $DumpDir 'field.vram'

Write-Host '=== Mesen 2 — SNES Video RAM export ==='
Write-Host "Executable: $MesenExe"
Write-Host "ROM:        $ShortRom"
Write-Host "Target:     $DumpPath (65536 bytes)"
Write-Host ''
Write-Host 'If you see NO menu bar:'
Write-Host '  - Press F11 to leave fullscreen (menu is always hidden in fullscreen).'
Write-Host '  - Press Alt once, or move the mouse to the very top edge of the window.'
Write-Host '  - Options -> Preferences -> General: turn OFF "Automatically hide the menu bar".'
Write-Host ''
Write-Host 'Export steps (after title screen or first-person field view):'
Write-Host '  1. Debug -> Memory Tools   (shortcut: Ctrl+M)'
Write-Host '  2. In the Memory Tools window, open the memory-type dropdown at the top.'
Write-Host '     Select "Video RAM" — NOT "SNES Memory" (default).'
Write-Host '     Wrong choice exports 16777216 bytes as *.dmp and is useless for RE.'
Write-Host '  3. In the SAME Memory Tools window: File -> Export   (shortcut: Ctrl+S)'
Write-Host '     Save as field.vram under dumps/ (not the main emulator File menu).'
Write-Host '  4. Confirm size is exactly 65536 bytes.'
Write-Host ''
Write-Host 'Compare:'
Write-Host "  python tools\\snes_vram_dump_stub.py --dump `"$DumpPath`" --compare-staging"
Write-Host ''
Write-Host 'ZSNES fallback (save-state extract): powershell tools\\snes_zsnes_dump.ps1 -Pal -Seconds 90'
Write-Host ''

if (-not $NoLaunch) {
  Start-Process -FilePath $MesenExe -WorkingDirectory (Split-Path $MesenExe -Parent) -ArgumentList "`"$ShortRom`"" -WindowStyle Normal
}
