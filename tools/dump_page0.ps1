$map = [System.IO.File]::ReadAllBytes('c:\_20260421_\D-REC\development\MM2\map.dat')
$data = [System.IO.File]::ReadAllBytes('c:\_20260421_\D-REC\development\MM2\EXTRACTED\ghidra\mm2_data_00.bin')

# Screen 0, page 0 = offset 0 in map.dat (256 bytes)
Write-Host "=== Screen 0, Page 0 (visual) - 16x16 grid ==="
Write-Host "Row (Y) goes 0=south to 15=north. Hex values:"
for ($y = 15; $y -ge 0; $y--) {
    $row = ""
    for ($x = 0; $x -le 15; $x++) {
        $idx = $y * 16 + $x
        $v = $map[$idx]
        $row += ("{0:X2} " -f $v)
    }
    Write-Host ("Y{0:D2}: {1}" -f $y, $row)
}

Write-Host ""
Write-Host "=== Bits 7-6 (mask 0xC0) at each cell - what facing-N sees ==="
for ($y = 15; $y -ge 0; $y--) {
    $row = ""
    for ($x = 0; $x -le 15; $x++) {
        $idx = $y * 16 + $x
        $v = $map[$idx]
        $bits = ($v -band 0xC0) -shr 6
        $row += "$bits "
    }
    Write-Host ("Y{0:D2}: {1}" -f $y, $row)
}

Write-Host ""
Write-Host "=== kCartoTile table (data @A4-$765E, 64 bytes) ==="
$a4 = 0x7FFE
$off = $a4 - 0x765E
Write-Host ("offset in data hunk: +{0:X4}" -f $off)
$vals = @()
for ($i = 0; $i -lt 64; $i++) { $vals += $data[$off + $i] }
Write-Host ($vals -join ' ')

Write-Host ""
Write-Host "=== kCartoTile[byte>>2] for each unique visual byte in screen 0 ==="
$seen = @{}
for ($i = 0; $i -lt 256; $i++) {
    $v = $map[$i]
    if (-not $seen.ContainsKey($v)) {
        $seen[$v] = $true
        $idx2 = $v -shr 2
        $ct = if ($idx2 -lt 64) { $data[$off + $idx2] } else { "OOB" }
        $bits76 = ($v -band 0xC0) -shr 6
        $bits54 = ($v -band 0x30) -shr 4
        $bits32 = ($v -band 0x0C) -shr 2
        $bits10 = ($v -band 0x03)
        Write-Host ("  byte={0:X2} >> 2={1:D2} cartoTile={2}  N={3} E={4} S={5} W={6}" -f $v, $idx2, $ct, $bits76, $bits54, $bits32, $bits10)
    }
}
