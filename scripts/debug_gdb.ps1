# debug_gdb.ps1 - boot frozen, attach gdb, break on the kernel exception entry, type /bin/sh
param([int]$BootWaitSec = 40, [int]$MonitorPort = 4453, [int]$GdbPort = 1234)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
$QemuPath = 'C:\Program Files\qemu\qemu-system-x86_64.exe'
$GdbPath = Join-Path $PSScriptRoot '..\tools\w64devkit\bin\gdb.exe'

Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process gdb -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
Remove-Item build\serial.log -ErrorAction SilentlyContinue
Remove-Item build\gdb.log,build\gdb.err,build\gdb_out.log -ErrorAction SilentlyContinue

$q = Start-Process -WindowStyle Hidden -FilePath $QemuPath -ArgumentList @(
    '-accel','whpx','-accel','tcg',
    '-cpu','max','-m','512M',
    '-drive','format=raw,file=build\os.img',
    '-drive','format=raw,file=build\disk.img,if=ide,index=1',
    '-vga','std','-global','VGA.vgamem_mb=64',
    '-display','none',
    '-serial','file:build/serial.log',
    '-monitor',"telnet:127.0.0.1:$MonitorPort,server,nowait",
    '-S','-s'
) -PassThru
Start-Sleep -Seconds 2

$gdbCmds = @"
set pagination off
set confirm off
set architecture i386:x86-64
set remotetimeout 120
set logging file build/gdb_out.log
set logging enabled on
target remote 127.0.0.1:$GdbPort
hbreak *0xFFFF8000000123A6
continue
printf "=== FAULT FRAME ===\n"
info registers rip rsp rax rbx rcx rdx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15
printf "=== STACK ===\n"
x/48gx `$rsp
printf "=== CODE AT FAULT ===\n"
x/12i `$rip-24
detach
quit
"@
Set-Content build\gdb.cmds $gdbCmds
$g = Start-Process -WindowStyle Hidden -FilePath $GdbPath -ArgumentList @('-batch','-x','build\gdb.cmds') -RedirectStandardOutput build\gdb.log -RedirectStandardError build\gdb.err -PassThru

Start-Sleep -Seconds $BootWaitSec
try {
    $c = New-Object System.Net.Sockets.TcpClient('127.0.0.1', $MonitorPort)
    $s = $c.GetStream()
    function Send([string]$cmd) {
        $b = [Text.Encoding]::ASCII.GetBytes("$cmd`n")
        $s.Write($b, 0, $b.Length)
        Start-Sleep -Milliseconds 160
    }
    $KeyMap = @{ ' ' = 'spc'; '/' = 'slash'; '.' = 'dot'; '-' = 'minus'; '_' = 'shift-minus' }
    function SendText([string]$text) {
        foreach ($ch in $text.ToCharArray()) {
            $key = if ($KeyMap.ContainsKey([string]$ch)) { $KeyMap[[string]$ch] }
                   elseif ([char]::IsUpper($ch)) { 'shift-' + $ch.ToString().ToLower() }
                   else { $ch.ToString() }
            Send "sendkey $key"
        }
        Send 'sendkey ret'
    }
    SendText '/bin/sh'
    Start-Sleep -Seconds 12
    SendText 'echo GOT:sh'
    Start-Sleep -Seconds 8
} catch {
    Write-Host "monitor error: $_"
}

Start-Sleep -Seconds 2
if (-not $g.HasExited) { Start-Sleep -Seconds 8 }
if (-not $g.HasExited) { Stop-Process -Id $g.Id -Force }
Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force

Write-Host '--- gdb log ---'
if (Test-Path build\gdb.log) { Get-Content build\gdb.log }
Write-Host '--- gdb out ---'
if (Test-Path build\gdb_out.log) { Get-Content build\gdb_out.log }
Write-Host '--- serial tail ---'
Get-Content build\serial.log -Tail 12
