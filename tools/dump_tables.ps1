$f = [System.IO.File]::ReadAllBytes('c:\_20260421_\D-REC\development\MM2\EXTRACTED\ghidra\mm2_data_00.bin')
$a4 = 0x7FFE

function dump16($base, $n, $label) {
    $off = $a4 - $base
    Write-Host "$label @ A4-$($base.ToString('X4')) [+$($off.ToString('X4'))]:"
    $vals = @()
    for ($i=0; $i -lt $n; $i++) {
        $v = [int16][System.BitConverter]::ToInt16([byte[]]@($f[$off+$i*2+1], $f[$off+$i*2]), 0)
        $vals += $v
    }
    Write-Host ("  " + ($vals -join ', '))
}

function dump8($base, $n, $label) {
    $off = $a4 - $base
    Write-Host "$label @ A4-$($base.ToString('X4')) [+$($off.ToString('X4'))]:"
    $vals = @()
    for ($i=0; $i -lt $n; $i++) { $vals += $f[$off+$i] }
    Write-Host ("  " + ($vals -join ', '))
}

# Wall piece position/frame tables (from wall_piece_* ASM lea addresses)
dump16 0x75AE 4 "FrontX    "
dump16 0x75B6 4 "FrontY    "
dump16 0x75A2 4 "LeftX1    "
dump16 0x759A 4 "LeftY1    "
dump8  0x75A6 4 "LeftFrmB  "
dump16 0x7592 4 "LeftX2    "
dump16 0x758A 4 "LeftY2    "
dump16 0x757E 4 "RightX1   "
dump16 0x7576 4 "RightY1   "
dump8  0x7582 4 "RightFrmB "
dump16 0x756E 4 "RightX2   "
dump16 0x7566 4 "RightY2   "

# Direction bundles (from map_neighbourhood_refresh @0x19D6)
# N='N'(0x4E), S='S'(0x53), E='E'(0x45), W='W'(0x57)
dump8 0x7646 6 "BundleN   "
dump8 0x7640 6 "BundleS   "
dump8 0x763A 6 "BundleE   "
dump8 0x7634 6 "BundleW   "
