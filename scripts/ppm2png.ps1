# ppm2png.ps1 - convert a QEMU P6 PPM screendump to PNG
param([string]$In = 'build\dbg.ppm', [string]$Out = 'build\dbg.png')
$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path $In))
$pos = 0
function NextToken([ref]$p) {
    $b = $bytes
    while ($b[$p.Value] -eq 32 -or $b[$p.Value] -eq 10 -or $b[$p.Value] -eq 13 -or $b[$p.Value] -eq 9) { $p.Value++ }
    $s = $p.Value
    while ($b[$p.Value] -ne 32 -and $b[$p.Value] -ne 10 -and $b[$p.Value] -ne 13 -and $b[$p.Value] -ne 9) { $p.Value++ }
    return [Text.Encoding]::ASCII.GetString($b[$s..($p.Value - 1)])
}
$magic = NextToken ([ref]$pos)
$w = [int](NextToken ([ref]$pos))
$h = [int](NextToken ([ref]$pos))
$max = NextToken ([ref]$pos)
$pos++
$rowPad = (4 - ($w * 3) % 4) % 4
$rowSize = $w * 3 + $rowPad
$bmpSize = 54 + $rowSize * $h
$bmp = New-Object byte[] $bmpSize
$bmp[0] = 0x42; $bmp[1] = 0x4D
[BitConverter]::GetBytes([int]$bmpSize).CopyTo($bmp, 2)
[BitConverter]::GetBytes([int]54).CopyTo($bmp, 10)
[BitConverter]::GetBytes([int]40).CopyTo($bmp, 14)
[BitConverter]::GetBytes([int]$w).CopyTo($bmp, 18)
[BitConverter]::GetBytes([int]$h).CopyTo($bmp, 22)
[BitConverter]::GetBytes([int16]1).CopyTo($bmp, 26)
[BitConverter]::GetBytes([int16]24).CopyTo($bmp, 28)
[BitConverter]::GetBytes([int]$rowSize * $h).CopyTo($bmp, 34)
for ($y = 0; $y -lt $h; $y++) {
    $src = $pos + ($h - 1 - $y) * $w * 3
    $dst = 54 + $y * $rowSize
    for ($x = 0; $x -lt $w; $x++) {
        $bmp[$dst + $x * 3]     = $bytes[$src + $x * 3 + 2]
        $bmp[$dst + $x * 3 + 1] = $bytes[$src + $x * 3 + 1]
        $bmp[$dst + $x * 3 + 2] = $bytes[$src + $x * 3]
    }
}
Add-Type -AssemblyName System.Drawing
$ms = New-Object System.IO.MemoryStream(,$bmp)
$img = [System.Drawing.Image]::FromStream($ms)
$bmp2 = New-Object System.Drawing.Bitmap($img)
$bmp2.Save((Join-Path (Get-Location) $Out), [System.Drawing.Imaging.ImageFormat]::Png)
$img.Dispose(); $bmp2.Dispose(); $ms.Dispose()
Write-Host "wrote $Out"
