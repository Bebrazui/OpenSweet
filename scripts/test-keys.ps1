# Opensweet OS - headless test: boot, inject PS/2 keys via QEMU monitor, dump serial log
param(
    [string]$KeysCsv = 'd i v ret',
    [int]$BootWaitSec = 3,
    [int]$SettleMS = 300,
    [string]$MonitorPort = '4444'
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot\..
$Keys = $KeysCsv -split '[\s,]+' | Where-Object { $_ }

Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1
Remove-Item build\serial.log -ErrorAction SilentlyContinue

$qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$q = Start-Process -FilePath $qemu -ArgumentList `
    "-drive","format=raw,file=build\os.img", "-drive","format=raw,file=build\disk.img,if=ide,index=1", `
    "-display","none", `
    "-serial","file:build\serial.log", `
    "-monitor","telnet:127.0.0.1:$MonitorPort`,server,nowait" -PassThru

Start-Sleep -Seconds $BootWaitSec

try {
    $c = New-Object System.Net.Sockets.TcpClient('127.0.0.1', $MonitorPort)
    $s = $c.GetStream()
    foreach ($k in $Keys) {
        $b = [Text.Encoding]::ASCII.GetBytes("sendkey $k`n")
        $s.Write($b, 0, $b.Length)
        Start-Sleep -Milliseconds $SettleMS
    }
    Start-Sleep -Seconds 2
    $b = [Text.Encoding]::ASCII.GetBytes("quit`n")
    $s.Write($b, 0, $b.Length)
    Start-Sleep -Seconds 1
    $c.Close()
} finally {
    Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
}

if (Test-Path build\serial.log) { Get-Content build\serial.log } else { "NO SERIAL LOG - qemu failed to start?" }
