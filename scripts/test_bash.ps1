# Opensweet OS - drive the Debian bash test from the QEMU monitor.
#
# Replaces scripts/test_bash_deb.py for machines without Python + Pillow:
# boots headless, types the test steps into the PS/2 console through the QEMU
# monitor, saves a screenshot and dumps the tail of the serial log.
#
# Example:
#   powershell -File scripts/test_bash.ps1
#   powershell -File scripts/test_bash.ps1 -UnpackWaitSec 90 -SkipUnpack
#
# Environment:
#   OPENSWEET_QEMU - path to qemu-system-x86_64.exe

param(
    [string]$QemuPath = $(if ($env:OPENSWEET_QEMU) { $env:OPENSWEET_QEMU } else { 'C:\Program Files\qemu\qemu-system-x86_64.exe' }),
    [string]$MonitorPort = '4445',
    [int]$BootWaitSec = 15,
    [int]$UnpackWaitSec = 180,
    [int]$StepWaitSec = 12,
    [switch]$SkipUnpack,
    [switch]$RebuildDisk,
    [int]$SerialTail = 60
)

$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')

if (-not (Test-Path $QemuPath)) { Write-Error "qemu not found at '$QemuPath'"; exit 1 }
foreach ($img in @('build\os.img', 'build\disk.img')) {
    if (-not (Test-Path $img)) { Write-Error "$img missing - run build.cmd and make-ext4.ps1"; exit 1 }
}

# A failed extraction leaves partial files behind, so repeated runs against the
# same disk.img are not comparable. -RebuildDisk gives a clean filesystem.
if ($RebuildDisk) {
    Write-Host 'Rebuilding build\disk.img ...' -ForegroundColor Cyan
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot '..\make-ext4.ps1')
    if ($LASTEXITCODE -ne 0) { Write-Error 'make-ext4.ps1 failed'; exit 1 }
}

# QEMU monitor key names for the characters the test needs.
$KeyMap = @{
    ' ' = 'spc'; '/' = 'slash'; '.' = 'dot'; '-' = 'minus'; '_' = 'shift-minus'
}

function Send([System.Net.Sockets.NetworkStream]$s, [string]$cmd) {
    $b = [Text.Encoding]::ASCII.GetBytes("$cmd`n")
    $s.Write($b, 0, $b.Length)
    Start-Sleep -Milliseconds 120
}

function SendText([System.Net.Sockets.NetworkStream]$s, [string]$text) {
    foreach ($ch in $text.ToCharArray()) {
        $key = if ($KeyMap.ContainsKey([string]$ch)) { $KeyMap[[string]$ch] }
               elseif ([char]::IsUpper($ch)) { 'shift-' + $ch.ToString().ToLower() }
               else { $ch.ToString() }
        Send $s "sendkey $key"
    }
    Send $s 'sendkey ret'
}

Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item build\serial.log -ErrorAction SilentlyContinue

Write-Host 'Booting...' -ForegroundColor Cyan
$q = Start-Process -WindowStyle Hidden -FilePath $QemuPath -ArgumentList `
    '-accel', 'whpx', '-accel', 'tcg', `
    '-cpu', 'max', '-m', '512M', `
    '-drive', 'format=raw,file=build\os.img', `
    '-drive', 'format=raw,file=build\disk.img,if=ide,index=1', `
    '-vga', 'std', '-global', 'VGA.vgamem_mb=64', `
    '-display', 'none', `
    '-serial', 'file:build\serial.log', `
    '-monitor', "telnet:127.0.0.1:$MonitorPort,server,nowait" -PassThru

Start-Sleep -Seconds $BootWaitSec
$c = New-Object System.Net.Sockets.TcpClient('127.0.0.1', $MonitorPort)
$s = $c.GetStream()

try {
    if (-not $SkipUnpack) {
        Write-Host 'Step 1: dpkg-deb -x /bash.deb /' -ForegroundColor Cyan
        SendText $s 'dpkg-deb -x /bash.deb /'
        Start-Sleep -Seconds $UnpackWaitSec
    }

    Write-Host 'Step 2: ls -l /bin/bash' -ForegroundColor Cyan
    SendText $s 'ls /bin'
    Start-Sleep -Seconds $StepWaitSec

    Write-Host 'Step 3: /bin/bash --version' -ForegroundColor Cyan
    SendText $s '/bin/bash --version'
    Start-Sleep -Seconds ($StepWaitSec + 6)

    Send $s 'screendump build\bash_ps_result.ppm'
    Start-Sleep -Seconds 2
} finally {
    try { Send $s 'quit' } catch {}
    Start-Sleep -Seconds 1
    try { $c.Close() } catch {}
    Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
}

Write-Host "`n--- serial (unhandled syscalls) ---" -ForegroundColor Yellow
$unhandled = Select-String -Path build\serial.log -Pattern 'Unhandled syscall' -ErrorAction SilentlyContinue
if ($unhandled) { $unhandled | ForEach-Object { $_.Line.Trim() } | Group-Object | ForEach-Object { "{0,4}x  {1}" -f $_.Count, $_.Name } }
else { 'none' }

Write-Host "`n--- serial tail ---" -ForegroundColor Yellow
Get-Content build\serial.log -Tail $SerialTail -ErrorAction SilentlyContinue
Write-Host "`nScreenshot: build\bash_ps_result.ppm"
