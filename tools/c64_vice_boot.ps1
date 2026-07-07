# Launch VICE x64sc with MM2-1.D64 (interactive GUI boot).
param(
  [int] $Disk = 1,
  [switch] $TrueDrive,
  [switch] $Warp
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
$D64 = Join-Path $Root "EXTRACTED\c64\MM2-$Disk.D64"
if (-not (Test-Path $D64)) { throw "Missing $D64" }

$ViceDir = $env:VICE_ROOT
if (-not $ViceDir) {
  $cmd = Get-Command x64sc.exe -ErrorAction SilentlyContinue
  if ($cmd) { $ViceDir = Split-Path $cmd.Source -Parent }
}
if (-not $ViceDir) {
  throw 'Set $env:VICE_ROOT to SDL2VICE install dir or add x64sc.exe to PATH (winget VICE-Team.VICE.GTK3).'
}
$X64 = Join-Path $ViceDir 'x64sc.exe'

$Args = @(
  '-autostart', $D64,
  '-autostart-delay', '2',
  '-autostart-handle-tde',
  '-drive8type', '1541',
  '+confirmonexit'
)
if ($TrueDrive) { $Args += '-drive8truedrive' }
if ($Warp) { $Args += @('-autostart-warp', '-warp') }

Write-Host "Starting: $X64 $($Args -join ' ')"
Push-Location $ViceDir
try {
  & $X64 @Args
} finally {
  Pop-Location
}
