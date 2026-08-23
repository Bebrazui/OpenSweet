# Opensweet OS - generate minimal ext4 image (1KB blocks, extents-only) for testing
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (-not (Test-Path build)) { New-Item -ItemType Directory build | Out-Null }

$img = New-Object byte[] 16777216          # 16MB

function U16([int]$v) { [byte[]]@(($v -band 0xFF), (($v -shr 8) -band 0xFF)) }
function U32([long]$v) {
    [byte[]]@(($v -band 0xFF), (($v -shr 8) -band 0xFF), (($v -shr 16) -band 0xFF), (($v -shr 24) -band 0xFF))
}
function Put([long]$off, [byte[]]$b) { [Array]::Copy($b, 0, $img, $off, $b.Length) }
function PutU16([long]$off, [int]$v) { Put $off (U16 $v) }
function PutU32([long]$off, [long]$v) { Put $off (U32 $v) }

$BS = 1024                                  # block size

# ---- superblock @ byte 1024 ----
$sb = 1024
PutU32 ($sb + 0x00) 32                      # inodes_count
PutU32 ($sb + 0x04) 16384                   # blocks_count_lo
PutU32 ($sb + 0x14) 1                       # first_data_block (1KB blocks)
PutU32 ($sb + 0x18) 0                       # log_block_size = 0 -> 1024
PutU32 ($sb + 0x20) 16384                   # blocks_per_group (one group)
PutU32 ($sb + 0x28) 32                      # inodes_per_group
PutU16 ($sb + 0x36) 1                       # state = clean
PutU16 ($sb + 0x38) 0xEF53                  # magic
PutU32 ($sb + 0x4C) 1                       # rev_level = dynamic
PutU32 ($sb + 0x54) 11                      # first_ino
PutU16 ($sb + 0x58) 128                     # inode_size
PutU32 ($sb + 0x5C) 0                       # feature_compat
PutU32 ($sb + 0x60) 0x42                    # incompatible: FILETYPE|EXTENTS
PutU32 ($sb + 0x64) 0                       # ro_compat

# ---- GDT @ block 2: one group descriptor ----
$gdt = 2 * $BS
PutU32 ($gdt + 0)  3                        # bg_block_bitmap
PutU32 ($gdt + 4)  4                        # bg_inode_bitmap
PutU32 ($gdt + 8)  10                       # bg_inode_table (blocks 10..13)

# ---- helper: write an extents-root inode into inode table ----
function ExtentLeaf([long]$inoOff, [long]$fsBlock, [int]$len, [int]$size, [int]$mode) {
    PutU16 $inoOff 0xF30A                   # eh_magic
    PutU16 ($inoOff + 2) 1                  # eh_entries
    PutU16 ($inoOff + 4) 3                  # eh_max
    PutU16 ($inoOff + 6) 0                  # eh_depth
    PutU32 ($inoOff + 8) 0                  # eh_generation
    PutU32 ($inoOff + 12) 0                 # ee_block
    PutU16 ($inoOff + 16) $len              # ee_len
    PutU16 ($inoOff + 18) 0                 # ee_start_hi
    PutU32 ($inoOff + 20) $fsBlock          # ee_start_lo
    PutU16 $inoOff $mode                    # i_mode (overwrite low bytes)
    PutU16 ($inoOff + 2) 0                  # uid
    PutU32 ($inoOff + 4) $size              # i_size_lo
    PutU32 ($inoOff + 32) 0x80000           # i_flags = EXTENTS
}

$itable = 10 * $BS
# inode 2 = root dir -> block 20
ExtentLeaf ($itable + 1 * 128) 20 1 1024 0x41ED
# inode 11 = hello.txt -> block 21
ExtentLeaf ($itable + 10 * 128) 21 1 26 0x81A4
# inode 12 = big.txt -> blocks 22..25 (extent len 4)
ExtentLeaf ($itable + 11 * 128) 22 4 4096 0x81A4

# ---- root dir data @ block 20 ----
$d = 20 * $BS
function DirEntry([long]$off, [int]$ino, [string]$name, [byte]$type, [int]$reclen) {
    PutU32 $off $ino
    PutU16 ($off + 4) $reclen
    $nb = [Text.Encoding]::ASCII.GetBytes($name)
    $img[$off + 6] = $nb.Length
    $img[$off + 7] = $type
    Put ($off + 8) $nb
}
DirEntry $d        2  '.'        2 12
DirEntry ($d + 12) 2  '..'       2 12
DirEntry ($d + 24) 11 'hello.txt' 1 20
DirEntry ($d + 44) 12 'big.txt'   1 (1024 - 44)

# ---- file data ----
$hello = [Text.Encoding]::ASCII.GetBytes("Hello from Opensweet ext4!`n")
Put (21 * $BS) $hello
$pat = [Text.Encoding]::ASCII.GetBytes("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
for ($i = 0; $i -lt 4096; $i++) { $img[(22 * $BS) + $i] = $pat[$i % $pat.Length] }

[IO.File]::WriteAllBytes('build\disk.img', $img)
echo "Build OK: build\disk.img (minimal ext4, 1KB blocks)"
