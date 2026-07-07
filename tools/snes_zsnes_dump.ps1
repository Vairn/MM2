# Launch ZSNES for MM2 SNES VRAM validation (manual F2 save state step required).
param(
  [string] $ZsnesDir = 'C:\Users\Adam Templeton\Downloads\SNES Gems - The Ultimate ROM Collection\SNES Gems - The Ultimate ROM Collection\zsnes1-40',
  [string] $Rom = '',
  [int] $Seconds = 45,
  [switch] $Pal,
  [switch] $ExtractIfZst
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path $PSScriptRoot -Parent
if (-not $Rom) {
  $Rom = Join-Path $RepoRoot 'EXTRACTED\SNES\Might and Magic II (Europe).sfc'
}
$EmuDir = Join-Path $RepoRoot 'EXTRACTED\snes\emulator'
$DumpDir = Join-Path $EmuDir 'dumps'
New-Item -ItemType Directory -Force -Path $EmuDir, $DumpDir | Out-Null

$ZsnesExe = Join-Path $ZsnesDir 'zsnesw.exe'
if (-not (Test-Path $ZsnesExe)) { throw "zsnesw.exe not found: $ZsnesExe" }
if (-not (Test-Path $Rom)) { throw "ROM not found: $Rom" }

$ShortRom = Join-Path $EmuDir 'mm2.sfc'
Copy-Item $Rom $ShortRom -Force

$Args = @('-m', '-a', '-f', '2', '-s')
if ($Pal) { $Args += '-u' } else { $Args += '-t' }
$Args += "`"$ShortRom`""

Write-Host "Starting ZSNES ($ZsnesExe)"
Write-Host "ROM: $ShortRom"
Write-Host ""
Write-Host "MANUAL: when title or field view is visible, press F2 to save state."
Write-Host "Save file expected beside ROM: mm2.zst (or mm2.zs0..zs9 depending on slot)."
Write-Host "Waiting $Seconds s then closing emulator..."
Write-Host ""

$proc = Start-Process -FilePath $ZsnesExe -WorkingDirectory $ZsnesDir -PassThru -WindowStyle Normal -ArgumentList $Args
Start-Sleep -Seconds $Seconds
if (-not $proc.HasExited) {
  Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}

Copy-Item (Join-Path $ZsnesDir 'rominfo.txt') (Join-Path $EmuDir 'rominfo.txt') -Force -ErrorAction SilentlyContinue

$zst = Get-ChildItem $EmuDir -Filter 'mm2.z*' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($zst) {
  Write-Host "Found save state: $($zst.FullName)"
  if ($ExtractIfZst) {
    python (Join-Path $PSScriptRoot 'snes_zst_extract.py') $zst.FullName -o $DumpDir --compare-staging --json (Join-Path $EmuDir 'staging_compare.json')
  }
} else {
  Write-Warning 'No mm2.z* save state found — F2 was not captured or save went elsewhere.'
  Write-Host 'Check zsnesw.cfg SaveDirectory and retry with focus on the ZSNES window.'
}
