param(
    [Parameter(Mandatory=$true)][string]$In,
    [string]$Out
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if (-not $Out) { $Out = [IO.Path]::ChangeExtension($In, '.png') }

$bytes = [IO.File]::ReadAllBytes($In)
# parse P6 header: magic, whitespace-separated w h maxval, single whitespace then data
$i = 0
function NextTok([byte[]]$b, [ref]$i) {
    while ($i.Value -lt $b.Length -and ([char]$b[$i.Value] -match '\s')) { $i.Value++ }
    if ($b[$i.Value] -eq [byte][char]'#') { while ($i.Value -lt $b.Length -and $b[$i.Value] -ne 10) { $i.Value++ }; return (NextTok $b $i) }
    $s = ''
    while ($i.Value -lt $b.Length -and -not ([char]$b[$i.Value] -match '\s')) { $s += [char]$b[$i.Value]; $i.Value++ }
    return $s
}
$magic = NextTok $bytes ([ref]$i)
if ($magic -ne 'P6') { throw "not P6: $magic" }
$w = [int](NextTok $bytes ([ref]$i))
$h = [int](NextTok $bytes ([ref]$i))
$max = [int](NextTok $bytes ([ref]$i))
$i++  # single whitespace after maxval
$bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$rect = New-Object System.Drawing.Rectangle(0, 0, $w, $h)
$data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$stride = $data.Stride
$buf = New-Object byte[] ($stride * $h)
for ($y = 0; $y -lt $h; $y++) {
    for ($x = 0; $x -lt $w; $x++) {
        $src = $i + ($y * $w + $x) * 3
        $dst = $y * $stride + $x * 3
        # Bitmap 24bppRgb stores BGR
        $buf[$dst]     = $bytes[$src + 2]
        $buf[$dst + 1] = $bytes[$src + 1]
        $buf[$dst + 2] = $bytes[$src]
    }
}
[Runtime.InteropServices.Marshal]::Copy($buf, 0, $data.Scan0, $buf.Length)
$bmp.UnlockBits($data)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "Wrote $Out ($w x $h)"
