# test_term_input.ps1 - does the native terminal feed stdin to a child?
#
# Boots the OS headless, runs a child that reads from stdin and prints what it
# gets, types into the terminal with the QEMU monitor and screenshots the
# result.  Verifies that SYS_POLL + the interactive child loop in
# user/terminal.c actually deliver keystrokes.
param(
    [int]$MonitorPort = 4445,
    [int]$BootWaitSec = 25,
    [int]$StepWaitSec = 4,
    [int]$UnpackWaitSec = 180,
    [switch]$Bash,
    [switch]$RebuildDisk,
    [switch]$KeepRunning
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$QemuPath = 'C:\Program Files\qemu\qemu-system-x86_64.exe'
if (-not (Test-Path $QemuPath)) { $QemuPath = 'qemu-system-x86_64.exe' }

if ($RebuildDisk) {
    Write-Host 'Rebuilding disk image...' -ForegroundColor Cyan
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot '..\make-ext4.ps1') | Out-Null
}

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
    if ($Bash) {
        Write-Host 'Step 0: dpkg-deb -x /bash.deb / (extract Debian bash)' -ForegroundColor Cyan
        SendText $s 'dpkg-deb -x /bash.deb /'
        Start-Sleep -Seconds $UnpackWaitSec
        Send $s 'screendump build\term_unpack.ppm'
        Start-Sleep -Seconds 2

        Write-Host "Step 0b: /bin/bash -c 'echo from-bash-c'" -ForegroundColor Cyan
        SendText $s "/bin/bash -c 'echo from-bash-c'"
        Start-Sleep -Seconds ($StepWaitSec + 4)
        Send $s 'screendump build\term_bash_c.ppm'
        Start-Sleep -Seconds 2
    }

    # /bin/sh is busybox, present on the image without unpacking anything.
    # 'echo GOT:sh' has output different from the input line, so seeing GOT:sh
    # proves the keystrokes really reached the child's stdin and its stdout
    # came back through the pipe.
    Write-Host 'Step 1: /bin/sh (reads commands from stdin)' -ForegroundColor Cyan
    SendText $s '/bin/sh'
    Start-Sleep -Seconds ($StepWaitSec + 2)

    Write-Host 'Step 2: echo GOT:sh inside busybox sh' -ForegroundColor Cyan
    SendText $s 'echo GOT:sh'
    Start-Sleep -Seconds $StepWaitSec

    Send $s 'screendump build\term_input_bash.ppm'
    Start-Sleep -Seconds 2

    Write-Host 'Step 3: cat, then type into it' -ForegroundColor Cyan
    SendText $s 'cat'
    Start-Sleep -Seconds $StepWaitSec
    SendText $s 'hello world'
    Start-Sleep -Seconds $StepWaitSec

    Send $s 'screendump build\term_input_cat.ppm'
    Start-Sleep -Seconds 2
} finally {
    if (-not $KeepRunning) {
        try { Send $s 'quit' } catch {}
        Start-Sleep -Seconds 1
        try { $c.Close() } catch {}
        Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
    }
}

Write-Host "`n--- serial tail ---" -ForegroundColor Yellow
Get-Content build\serial.log -Tail 12 -ErrorAction SilentlyContinue
Write-Host "`nScreenshots: build\term_input_cat.ppm build\term_input_bash.ppm"
