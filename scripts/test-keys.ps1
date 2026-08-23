# Opensweet OS - headless test: boot, inject PS/2 keys via QEMU monitor, dump serial log
param(
    [string[]]$Keys = @('d','i','v','ret'),
    [int]$BootWaitSec = 3,
    [int]$SettleMS = 300,
    [string]$MonitorPort = '4444'
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot\..

Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1

$qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$q = Start-Process -FilePath $qemu -ArgumentList `
    "-drive","format=raw,file=build\os.img", `
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
    $c.Close()
} finally {
    Start-Sleep -Seconds 2
    Stop-Process -Id $q.Id -Force
}

"--- SERIAL LOG ---"
Get-Content build\serial.log
