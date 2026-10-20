# Minimal ext4 inspector for build\disk.img (1KB blocks, 64 inodes/group).
# Read-only debug helper: dumps superblock counters, group descriptors and every
# inode that is marked used, so a corrupted size / extent is visible directly.
param([string]$Image = 'build\disk.img')

$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')

$global:img = [IO.File]::ReadAllBytes((Resolve-Path $Image))
function U32([long]$o) { return [uint32][BitConverter]::ToUInt32($global:img, [int]$o) }
function U16([long]$o) { return [uint16][BitConverter]::ToUInt16($global:img, [int]$o) }

$sb = 1024
$blocks = U32 ($sb + 0x04)
$fdb = U32 ($sb + 0x14)
$bpg = U32 ($sb + 0x20)
$ipg = U32 ($sb + 0x28)
$isize = U16 ($sb + 0x58)
$groups = [int][math]::Ceiling(($blocks - $fdb) / $bpg)
$gdt = ($fdb + 1) * 1024

"blocks=$blocks first_data_block=$fdb bpg=$bpg ipg=$ipg isize=$isize groups=$groups"
"free_blocks={0} free_inodes={1}" -f (U32 ($sb + 0x0C)), (U32 ($sb + 0x10))

$ginfo = @()
for ($g = 0; $g -lt $groups; $g++) {
    $d = $gdt + $g * 32
    $gdesc = [pscustomobject]@{
        bbmp    = [long](U32 ($d + 0))
        ibmp    = [long](U32 ($d + 4))
        itable  = [long](U32 ($d + 8))
        freeblk = [int](U16 ($d + 0x0C))
        freeino = [int](U16 ($d + 0x0E))
    }
    $ginfo += $gdesc
    "group {0}: bbmp={1} ibmp={2} itable={3} freeblk={4} freeino={5} useddirs={6}" -f `
        $g, $gdesc.bbmp, $gdesc.ibmp, $gdesc.itable, $gdesc.freeblk, $gdesc.freeino, (U16 ($d + 0x10))
}

foreach ($g in 0..($groups - 1)) {
    $gd = $ginfo[$g]
    "--- group $g inode bitmap (block $($gd.ibmp)) ---"
    for ($i = 0; $i -lt $ipg; $i++) {
        $byte = $global:img[$gd.ibmp * 1024 + [int][math]::Floor($i / 8)]
        if ((($byte -shr ($i -band 7)) -band 1) -eq 0) { continue }
        $ino = $g * $ipg + $i + 1
        $o = $gd.itable * 1024 + $i * $isize
        $mode = U16 $o
        $size = [uint64](U32 ($o + 4)) -bor ([uint64](U32 ($o + 108)) -shl 32)
        $links = U16 ($o + 26)
        $flags = U32 ($o + 32)
        $iblocks = U32 ($o + 28)
        $ext = $o + 40
        $magic = U16 $ext
        $entries = U16 ($ext + 2)
        $depth = U16 ($ext + 6)
        "ino={0,3} mode={1:X4} size={2,8} links={3} flags={4:X8} iblocks={5,5} ext(magic={6:X4} n={7} d={8})" -f `
            $ino, $mode, $size, $links, $flags, $iblocks, $magic, $entries, $depth
        if ($magic -eq 0xF30A) {
            for ($e = 0; $e -lt [math]::Min([int]$entries, 3); $e++) {
                $p = $ext + 12 + $e * 12
                "        extent logical={0} start={1} len={2}" -f (U32 $p), (U32 ($p + 8)), (U16 ($p + 4))
            }
        }
    }
}
