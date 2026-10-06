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
PutU32 ($sb + 0x00) 128                     # inodes_count (64 * 2 groups)
PutU32 ($sb + 0x04) 16384                   # blocks_count_lo
PutU32 ($sb + 0x14) 1                       # first_data_block (1KB blocks)
PutU32 ($sb + 0x18) 0                       # log_block_size = 0 -> 1024
PutU32 ($sb + 0x20) 8192                    # blocks_per_group (standard 8192 blocks per group)
PutU32 ($sb + 0x28) 64                      # inodes_per_group
PutU16 ($sb + 0x36) 1                       # state = clean
PutU16 ($sb + 0x38) 0xEF53                  # magic
PutU32 ($sb + 0x4C) 1                       # rev_level = dynamic
PutU32 ($sb + 0x54) 11                      # first_ino
PutU16 ($sb + 0x58) 128                     # inode_size
PutU32 ($sb + 0x5C) 0                       # feature_compat
PutU32 ($sb + 0x60) 0x42                    # incompatible: FILETYPE|EXTENTS
PutU32 ($sb + 0x64) 0                       # ro_compat

# ---- GDT @ block 2: two group descriptors ----
$gdt = 2 * $BS
# Group 0:
PutU32 ($gdt + 0)  3                        # bg_block_bitmap
PutU32 ($gdt + 4)  4                        # bg_inode_bitmap
PutU32 ($gdt + 8)  10                       # bg_inode_table (blocks 10..17, 8 blocks)
# Group 1:
PutU32 ($gdt + 32 + 0) 8192                 # bg_block_bitmap
PutU32 ($gdt + 32 + 4) 8193                 # bg_inode_bitmap
PutU32 ($gdt + 32 + 8) 8194                 # bg_inode_table (blocks 8194..8201)

# ---- helper: write an extents-root inode into inode table ----
function ExtentLeaf([long]$inoOff, [long]$fsBlock, [int]$len, [int]$size, [int]$mode) {
    PutU16 $inoOff $mode                    # i_mode
    PutU16 ($inoOff + 2) 0                  # i_uid
    PutU32 ($inoOff + 4) $size              # i_size_lo
    PutU32 ($inoOff + 32) 0x80000           # i_flags = EXTENTS

    # extent header at i_block (offset 40)
    $ext = $inoOff + 40
    PutU16 $ext 0xF30A                      # eh_magic
    PutU16 ($ext + 2) 1                     # eh_entries
    PutU16 ($ext + 4) 4                     # eh_max
    PutU16 ($ext + 6) 0                     # eh_depth
    PutU32 ($ext + 8) 0                     # eh_generation
    PutU32 ($ext + 12) 0                    # ee_block
    PutU16 ($ext + 16) $len                 # ee_len
    PutU16 ($ext + 18) 0                    # ee_start_hi
    PutU32 ($ext + 20) $fsBlock             # ee_start_lo
}

$itable = 10 * $BS
# inode 2 = root dir -> block 20
ExtentLeaf ($itable + 1 * 128) 20 1 1024 0x41ED
# inode 11 = hello.txt -> block 21
ExtentLeaf ($itable + 10 * 128) 21 1 26 0x81A4
# inode 12 = big.txt -> blocks 22..25 (extent len 4)
ExtentLeaf ($itable + 11 * 128) 22 4 4096 0x81A4

$wallPath = Join-Path $PSScriptRoot "build\wallpaper.qoi"
if (-not (Test-Path $wallPath)) {
    $wallPath = Join-Path $PSScriptRoot "build\wallpaper.png"
}
$hasWall = Test-Path $wallPath
$wallBlocks = 0
$wallName = if ($wallPath.EndsWith(".qoi")) { "wallpaper.qoi" } else { "wallpaper.png" }
if ($hasWall) {
    $wallBytes = [IO.File]::ReadAllBytes($wallPath)
    $wallBlocks = [int][Math]::Ceiling($wallBytes.Length / 1024.0)
    # inode 13 = wallpaper -> blocks 26..(26 + wallBlocks - 1)
    ExtentLeaf ($itable + 12 * 128) 26 $wallBlocks $wallBytes.Length 0x81A4
}

# ---- compile and add hello.elf & gui_demo.elf ----
$fasmPath = "C:\Users\ttt79\Downloads\fasmw17335\FASM.EXE"
$helloAsm = Join-Path $PSScriptRoot "user\hello.asm"
$helloElf = Join-Path $PSScriptRoot "build\hello.elf"
if (Test-Path $helloAsm) {
    & $fasmPath $helloAsm $helloElf | Out-Null
}
$hasElf = Test-Path $helloElf
$elfBlocks = 0
$elfStartBlock = 26 + $wallBlocks
if ($hasElf) {
    $elfBytes = [IO.File]::ReadAllBytes($helloElf)
    $elfBlocks = [int][Math]::Ceiling($elfBytes.Length / 1024.0)
    # inode 14 = hello.elf -> blocks elfStartBlock..(elfStartBlock + elfBlocks - 1)
    ExtentLeaf ($itable + 13 * 128) $elfStartBlock $elfBlocks $elfBytes.Length 0x81ED
}

$guiAsm = Join-Path $PSScriptRoot "user\gui_demo.asm"
$guiElf = Join-Path $PSScriptRoot "build\gui_demo.elf"
if (Test-Path $guiAsm) {
    & $fasmPath $guiAsm $guiElf | Out-Null
}
$hasGui = Test-Path $guiElf
$guiBlocks = 0
$guiStartBlock = $elfStartBlock + $elfBlocks
if ($hasGui) {
    $guiBytes = [IO.File]::ReadAllBytes($guiElf)
    $guiBlocks = [int][Math]::Ceiling($guiBytes.Length / 1024.0)
    # inode 15 = gui_demo.elf -> blocks guiStartBlock..(guiStartBlock + guiBlocks - 1)
    ExtentLeaf ($itable + 14 * 128) $guiStartBlock $guiBlocks $guiBytes.Length 0x81ED
}

# ---- compile C applications (calc.elf, notepad.elf) ----
$gccPath = Join-Path $PSScriptRoot "tools\w64devkit\bin\gcc.exe"
$objcopyPath = Join-Path $PSScriptRoot "tools\w64devkit\bin\objcopy.exe"
$crt0 = Join-Path $PSScriptRoot "user\crt0.s"
$incDir = Join-Path $PSScriptRoot "include"

$calcSrc = Join-Path $PSScriptRoot "user\calc.c"
$calcExe = Join-Path $PSScriptRoot "build\calc.exe"
$calcElf = Join-Path $PSScriptRoot "build\calc.elf"
if (Test-Path $calcSrc) {
    & $gccPath "-B$(Split-Path $gccPath)" "-I$incDir" -mabi=sysv -nostdlib "-Wl,--image-base=0x400000" -O2 $crt0 $calcSrc -o $calcExe
    if (Test-Path $calcExe) {
        & $objcopyPath -O elf64-x86-64 $calcExe $calcElf
        Remove-Item $calcExe -ErrorAction SilentlyContinue
    }
}
$hasCalc = Test-Path $calcElf
$calcBlocks = 0
$calcStartBlock = $guiStartBlock + $guiBlocks
if ($hasCalc) {
    $calcBytes = [IO.File]::ReadAllBytes($calcElf)
    $calcBlocks = [int][Math]::Ceiling($calcBytes.Length / 1024.0)
    # inode 16 = calc.elf
    ExtentLeaf ($itable + 15 * 128) $calcStartBlock $calcBlocks $calcBytes.Length 0x81ED
}

$noteSrc = Join-Path $PSScriptRoot "user\notepad.c"
$noteExe = Join-Path $PSScriptRoot "build\notepad.exe"
$noteElf = Join-Path $PSScriptRoot "build\notepad.elf"
if (Test-Path $noteSrc) {
    & $gccPath "-B$(Split-Path $gccPath)" "-I$incDir" -mabi=sysv -nostdlib "-Wl,--image-base=0x400000" -O2 $crt0 $noteSrc -o $noteExe
    if (Test-Path $noteExe) {
        & $objcopyPath -O elf64-x86-64 $noteExe $noteElf
        Remove-Item $noteExe -ErrorAction SilentlyContinue
    }
}
$hasNote = Test-Path $noteElf
$noteBlocks = 0
$noteStartBlock = $calcStartBlock + $calcBlocks
if ($hasNote) {
    $noteBytes = [IO.File]::ReadAllBytes($noteElf)
    $noteBlocks = [int][Math]::Ceiling($noteBytes.Length / 1024.0)
    # inode 17 = notepad.elf
    ExtentLeaf ($itable + 16 * 128) $noteStartBlock $noteBlocks $noteBytes.Length 0x81ED
}

$termSrc = Join-Path $PSScriptRoot "user\terminal.c"
$termExe = Join-Path $PSScriptRoot "build\terminal.exe"
$termElf = Join-Path $PSScriptRoot "build\terminal.elf"
if (Test-Path $termSrc) {
    & $gccPath "-B$(Split-Path $gccPath)" "-I$incDir" -mabi=sysv -nostdlib "-Wl,--image-base=0x400000" -O2 $crt0 $termSrc -o $termExe
    if (Test-Path $termExe) {
        & $objcopyPath -O elf64-x86-64 $termExe $termElf
        Remove-Item $termExe -ErrorAction SilentlyContinue
    }
}
$hasTerm = Test-Path $termElf
$termBlocks = 0
$termStartBlock = $noteStartBlock + $noteBlocks
if ($hasTerm) {
    $termBytes = [IO.File]::ReadAllBytes($termElf)
    $termBlocks = [int][Math]::Ceiling($termBytes.Length / 1024.0)
    # inode 18 = terminal.elf
    ExtentLeaf ($itable + 17 * 128) $termStartBlock $termBlocks $termBytes.Length 0x81ED
}

$filesSrc = Join-Path $PSScriptRoot "user\files.c"
$filesExe = Join-Path $PSScriptRoot "build\files.exe"
$filesElf = Join-Path $PSScriptRoot "build\files.elf"
if (Test-Path $filesSrc) {
    & $gccPath "-B$(Split-Path $gccPath)" "-I$incDir" -mabi=sysv -nostdlib "-Wl,--image-base=0x400000" -O2 $crt0 $filesSrc -o $filesExe
    if (Test-Path $filesExe) {
        & $objcopyPath -O elf64-x86-64 $filesExe $filesElf
        Remove-Item $filesExe -ErrorAction SilentlyContinue
    }
}
$hasFiles = Test-Path $filesElf
$filesBlocks = 0
$filesStartBlock = $termStartBlock + $termBlocks
if ($hasFiles) {
    $filesBytes = [IO.File]::ReadAllBytes($filesElf)
    $filesBlocks = [int][Math]::Ceiling($filesBytes.Length / 1024.0)
    # inode 19 = files.elf
    ExtentLeaf ($itable + 18 * 128) $filesStartBlock $filesBlocks $filesBytes.Length 0x81ED
}

$linuxTestSrc = Join-Path $PSScriptRoot "user\linux_test.c"
$linuxTestExe = Join-Path $PSScriptRoot "build\linux_test.exe"
$linuxTestElf = Join-Path $PSScriptRoot "build\linux_test.elf"
if (Test-Path $linuxTestSrc) {
    # -e _start: без crt0 нет mainCRTStartup, иначе ld дефолтит entry на начало .text
    & $gccPath "-B$(Split-Path $gccPath)" -mabi=sysv -nostdlib "-Wl,--image-base=0x400000" "-Wl,-e,_start" -O2 $linuxTestSrc -o $linuxTestExe 2>$null
    if (Test-Path $linuxTestExe) {
        & $objcopyPath -O elf64-x86-64 $linuxTestExe $linuxTestElf
        Remove-Item $linuxTestExe -ErrorAction SilentlyContinue
    }
}
$hasLinuxTest = Test-Path $linuxTestElf
$linuxTestBlocks = 0
$linuxTestStartBlock = $filesStartBlock + $filesBlocks
if ($hasLinuxTest) {
    $linuxTestBytes = [IO.File]::ReadAllBytes($linuxTestElf)
    $linuxTestBlocks = [int][Math]::Ceiling($linuxTestBytes.Length / 1024.0)
    # inode 22 = linux_test.elf
    ExtentLeaf ($itable + 21 * 128) $linuxTestStartBlock $linuxTestBlocks $linuxTestBytes.Length 0x81ED
}

$busyboxPath = Join-Path $PSScriptRoot "build\busybox"
$hasBusybox = Test-Path $busyboxPath
$busyboxBlocks = 0
$busyboxStartBlock = if ($hasLinuxTest) { $linuxTestStartBlock + $linuxTestBlocks } else { $filesStartBlock + $filesBlocks }
if ($hasBusybox) {
    $busyboxBytes = [IO.File]::ReadAllBytes($busyboxPath)
    $busyboxBlocks = [int][Math]::Ceiling($busyboxBytes.Length / 1024.0)
    # inode 23 = busybox
    ExtentLeaf ($itable + 22 * 128) $busyboxStartBlock $busyboxBlocks $busyboxBytes.Length 0x81ED
}

$includeDoom = $true
$doomElf = Join-Path $PSScriptRoot "build\doom.elf"
if ($includeDoom -and (-not (Test-Path $doomElf))) {
    & cmd.exe /c "build_doom.cmd"
}
$hasDoom = $includeDoom -and (Test-Path $doomElf)
$doomBlocks = 0
$doomStartBlock = if ($hasBusybox) { $busyboxStartBlock + $busyboxBlocks } elseif ($hasLinuxTest) { $linuxTestStartBlock + $linuxTestBlocks } else { $filesStartBlock + $filesBlocks }
if ($hasDoom) {
    $doomBytes = [IO.File]::ReadAllBytes($doomElf)
    $doomBlocks = [int][Math]::Ceiling($doomBytes.Length / 1024.0)
    # inode 21 = doom.elf -> blocks doomStartBlock..(doomStartBlock + doomBlocks - 1)
    ExtentLeaf ($itable + 20 * 128) $doomStartBlock $doomBlocks $doomBytes.Length 0x81ED
}

$wadPath = Join-Path $PSScriptRoot "user\doomgeneric\doom1.wad"
$hasWad = $includeDoom -and (Test-Path $wadPath)
$wadBlocks = 0
$wadStartBlock = 8202
if ($hasWad) {
    $wadBytes = [IO.File]::ReadAllBytes($wadPath)
    $wadBlocks = [int][Math]::Ceiling($wadBytes.Length / 1024.0)
    # inode 20 = doom1.wad -> blocks wadStartBlock..(wadStartBlock + wadBlocks - 1)
    ExtentLeaf ($itable + 19 * 128) $wadStartBlock $wadBlocks $wadBytes.Length 0x81A4
}

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
DirEntry ($d + 44) 12 'big.txt'   1 20

$curOff = $d + 64
if ($hasWall) {
    DirEntry $curOff 13 $wallName 1 24
    $curOff += 24
}
if ($hasElf) {
    DirEntry $curOff 14 'hello.elf' 1 20
    $curOff += 20
}
if ($hasGui) {
    DirEntry $curOff 15 'gui_demo.elf' 1 20
    $curOff += 20
}
if ($hasCalc) {
    DirEntry $curOff 16 'calc.elf' 1 20
    $curOff += 20
}
if ($hasNote) {
    DirEntry $curOff 17 'notepad.elf' 1 24
    $curOff += 24
}
if ($hasTerm) {
    DirEntry $curOff 18 'terminal.elf' 1 24
    $curOff += 24
}
if ($hasFiles) {
    $rec = if ($hasLinuxTest -or $hasBusybox -or $hasDoom -or $hasWad) { 20 } else { $d + 1024 - $curOff }
    DirEntry $curOff 19 'files.elf' 1 $rec
    $curOff += 20
}
if ($hasLinuxTest) {
    $rec = if ($hasBusybox -or $hasDoom -or $hasWad) { 24 } else { $d + 1024 - $curOff }
    DirEntry $curOff 22 'linux_test.elf' 1 $rec
    $curOff += 24
}
if ($hasBusybox) {
    $rec = if ($hasDoom -or $hasWad) { 20 } else { $d + 1024 - $curOff }
    DirEntry $curOff 23 'busybox' 1 $rec
    $curOff += 20
}
if ($hasDoom) {
    $rec = if ($hasWad) { 20 } else { $d + 1024 - $curOff }
    DirEntry $curOff 21 'doom.elf' 1 $rec
    $curOff += 20
}
if ($hasWad) {
    DirEntry $curOff 20 'doom1.wad' 1 ($d + 1024 - $curOff)
}

# ---- file data ----
$hello = [Text.Encoding]::ASCII.GetBytes("Hello from Opensweet ext4!`n")
Put (21 * $BS) $hello
$pat = [Text.Encoding]::ASCII.GetBytes("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
for ($i = 0; $i -lt 4096; $i++) { $img[(22 * $BS) + $i] = $pat[$i % $pat.Length] }

if ($hasWall) {
    Put (26 * $BS) $wallBytes
    echo "Added $wallName ($($wallBytes.Length) bytes, $wallBlocks blocks) to disk image"
}
if ($hasElf) {
    Put ($elfStartBlock * $BS) $elfBytes
    echo "Added hello.elf ($($elfBytes.Length) bytes, $elfBlocks blocks) to disk image"
}
if ($hasGui) {
    Put ($guiStartBlock * $BS) $guiBytes
    echo "Added gui_demo.elf ($($guiBytes.Length) bytes, $guiBlocks blocks) to disk image"
}
if ($hasCalc) {
    Put ($calcStartBlock * $BS) $calcBytes
    echo "Added calc.elf ($($calcBytes.Length) bytes, $calcBlocks blocks) to disk image"
}
if ($hasNote) {
    Put ($noteStartBlock * $BS) $noteBytes
    echo "Added notepad.elf ($($noteBytes.Length) bytes, $noteBlocks blocks) to disk image"
}
if ($hasTerm) {
    Put ($termStartBlock * $BS) $termBytes
    echo "Added terminal.elf ($($termBytes.Length) bytes, $termBlocks blocks) to disk image"
}
if ($hasFiles) {
    Put ($filesStartBlock * $BS) $filesBytes
    echo "Added files.elf ($($filesBytes.Length) bytes, $filesBlocks blocks) to disk image"
}
if ($hasLinuxTest) {
    Put ($linuxTestStartBlock * $BS) $linuxTestBytes
    echo "Added linux_test.elf ($($linuxTestBytes.Length) bytes, $linuxTestBlocks blocks) to disk image"
}
if ($hasBusybox) {
    Put ($busyboxStartBlock * $BS) $busyboxBytes
    echo "Added busybox ($($busyboxBytes.Length) bytes, $busyboxBlocks blocks) to disk image"
}
if ($hasDoom) {
    Put ($doomStartBlock * $BS) $doomBytes
    echo "Added doom.elf ($($doomBytes.Length) bytes, $doomBlocks blocks) to disk image"
}
if ($hasWad) {
    Put ($wadStartBlock * $BS) $wadBytes
    echo "Added doom1.wad ($($wadBytes.Length) bytes, $wadBlocks blocks) to disk image"
}

# ---- populate block and inode bitmaps and free counts ----
$totalAllocatedBlocksGrp0 = if ($hasDoom) {
    $doomStartBlock + $doomBlocks
} elseif ($hasBusybox) {
    $busyboxStartBlock + $busyboxBlocks
} elseif ($hasLinuxTest) {
    $linuxTestStartBlock + $linuxTestBlocks
} elseif ($hasFiles) {
    $filesStartBlock + $filesBlocks
} else {
    26
}

# Block bitmap at block 3 (mark blocks 0..totalAllocatedBlocksGrp0-1 in group 0 as used)
$grp0Blocks = [Math]::Min($totalAllocatedBlocksGrp0, 8192)
for ($b = 0; $b -lt $grp0Blocks; $b++) {
    $byteIdx = $b -shr 3
    $bitIdx  = $b -band 7
    $img[(3 * $BS) + $byteIdx] = $img[(3 * $BS) + $byteIdx] -bor (1 -shl $bitIdx)
}

# Inode bitmap at block 4 (inodes 1..lastIno are used)
# Must cover the HIGHEST inode actually written above (index+1), else a
# free-but-used inode gets handed out by the allocator and corrupts a file.
$lastIno = if ($hasBusybox) { 23 } elseif ($hasLinuxTest) { 22 } elseif ($hasDoom) { 21 } elseif ($hasWad) { 20 } elseif ($hasFiles) { 19 } elseif ($hasTerm) { 18 } elseif ($hasNote) { 17 } elseif ($hasCalc) { 16 } elseif ($hasGui) { 15 } elseif ($hasElf) { 14 } elseif ($hasWall) { 13 } else { 12 }
[Array]::Clear($img, 4 * $BS, $BS)
for ($ino = 1; $ino -le $lastIno; $ino++) {
    $bit = $ino - 1
    $byteIdx = $bit -shr 3
    $bitIdx  = $bit -band 7
    $img[(4 * $BS) + $byteIdx] = $img[(4 * $BS) + $byteIdx] -bor (1 -shl $bitIdx)
}

# Group 1 block bitmap at block 8192:
# Mark 10 metadata blocks of Group 1 + wadBlocks
$grp1Blocks = if ($hasWad) { 10 + $wadBlocks } else { 10 }
for ($b = 0; $b -lt $grp1Blocks; $b++) {
    $byteIdx = $b -shr 3
    $bitIdx  = $b -band 7
    $img[(8192 * $BS) + $byteIdx] = $img[(8192 * $BS) + $byteIdx] -bor (1 -shl $bitIdx)
}

$freeBlocksGrp0 = 8192 - $grp0Blocks
$freeBlocksGrp1 = 8192 - $grp1Blocks
$freeBlocksTotal = $freeBlocksGrp0 + $freeBlocksGrp1

$freeInodesGrp0 = 64 - $lastIno
$freeInodesGrp1 = 64
$freeInodesTotal = $freeInodesGrp0 + $freeInodesGrp1

PutU32 ($sb + 0x0C) $freeBlocksTotal        # s_free_blocks_count_lo
PutU32 ($sb + 0x10) $freeInodesTotal        # s_free_inodes_count

# Group 0 descriptor:
PutU16 ($gdt + 0x0C) $freeBlocksGrp0        # bg_free_blocks_count_lo
PutU16 ($gdt + 0x0E) $freeInodesGrp0        # bg_free_inodes_count_lo
PutU16 ($gdt + 0x10) 1                      # bg_used_dirs_count_lo

# Group 1 descriptor:
PutU16 ($gdt + 32 + 0x0C) $freeBlocksGrp1   # bg_free_blocks_count_lo
PutU16 ($gdt + 32 + 0x0E) $freeInodesGrp1   # bg_free_inodes_count_lo
$diskOut = Join-Path $PSScriptRoot 'build\disk.img'
[IO.File]::WriteAllBytes($diskOut, $img)
echo "Build OK: $diskOut (minimal 2-group ext4, 1KB blocks)"


