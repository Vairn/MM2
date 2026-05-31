param([string]$file)
$f = [System.IO.File]::ReadAllBytes($file)
$frameCount = [uint16][System.BitConverter]::ToUInt16([byte[]]@($f[1], $f[0]), 0)
$depth      = [uint16][System.BitConverter]::ToUInt16([byte[]]@($f[3], $f[2]), 0)
Write-Host ("$file : frame_count=$frameCount  depth/mode=$depth")
for ($i = 0; $i -lt $frameCount; $i++) {
    $base = 4 + $i * 6
    $w = [uint16][System.BitConverter]::ToUInt16([byte[]]@($f[$base+1], $f[$base]), 0)
    $h = [uint16][System.BitConverter]::ToUInt16([byte[]]@($f[$base+3], $f[$base+2]), 0)
    Write-Host ("  frame[$i]: {0}x{1}" -f $w, $h)
}
