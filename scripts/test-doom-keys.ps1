# Scripts/test-doom-keys.ps1 - Test DOOM keyboard controls & navigation in OpenSweet OS
param(
    [string]$MonitorPort = '4444'
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot\..

Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1
Remove-Item build\serial.log -ErrorAction SilentlyContinue
Remove-Item build\doom_menu_down.ppm -ErrorAction SilentlyContinue
Remove-Item build\doom_gameplay.ppm -ErrorAction SilentlyContinue

$qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$q = Start-Process -FilePath $qemu -ArgumentList `
    "-m","512M", `
    "-drive","format=raw,file=build\os.img", "-drive","format=raw,file=build\disk.img,if=ide,index=1", `
    "-display","none", `
    "-serial","file:build\serial.log", `
    "-monitor","telnet:127.0.0.1:$MonitorPort`,server,nowait" -PassThru

Write-Host "Waiting for OpenSweet OS to boot..."
Start-Sleep -Seconds 3

try {
    $c = New-Object System.Net.Sockets.TcpClient('127.0.0.1', $MonitorPort)
    $s = $c.GetStream()

    function Send-Key($k, $sleepMs = 250) {
        $b = [Text.Encoding]::ASCII.GetBytes("sendkey $k`n")
        $s.Write($b, 0, $b.Length)
        Start-Sleep -Milliseconds $sleepMs
    }

    Write-Host "Launching DOOM..."
    foreach ($ch in ('d','o','o','m','ret')) {
        Send-Key $ch 200
    }

    Write-Host "Waiting 18s for WAD loading and title screen..."
    Start-Sleep -Seconds 18

    Write-Host "Opening menu and starting game: Enter -> Enter -> Enter -> Enter..."
    Send-Key 'ret' 1000
    Send-Key 'ret' 1000
    Send-Key 'ret' 1000
    Send-Key 'ret' 1500

    Write-Host "Waiting 4s for E1M1 level to spawn..."
    Start-Sleep -Seconds 4

    Write-Host "In-Game controls: testing simultaneous walk ('w:down') and camera turn ('left:down')..."
    # Hold 'w' down (start walking forward)
    $b = [Text.Encoding]::ASCII.GetBytes("sendkey w:down`n")
    $s.Write($b, 0, $b.Length)
    Start-Sleep -Milliseconds 400

    # While 'w' is held down, hold 'left' down (simultaneous walk + turn camera!)
    $b = [Text.Encoding]::ASCII.GetBytes("sendkey left:down`n")
    $s.Write($b, 0, $b.Length)
    Start-Sleep -Milliseconds 600

    # Release 'left' (stop turning)
    $b = [Text.Encoding]::ASCII.GetBytes("sendkey left:up`n")
    $s.Write($b, 0, $b.Length)
    Start-Sleep -Milliseconds 300

    # Fire pistol ('f') while still walking
    $b = [Text.Encoding]::ASCII.GetBytes("sendkey f`n")
    $s.Write($b, 0, $b.Length)
    Start-Sleep -Milliseconds 400

    # Release 'w' (stop walking)
    $b = [Text.Encoding]::ASCII.GetBytes("sendkey w:up`n")
    $s.Write($b, 0, $b.Length)
    Start-Sleep -Milliseconds 300

    Write-Host "Dumping E1M1 in-game screenshot..."
    $b = [Text.Encoding]::ASCII.GetBytes("screendump build\doom_e1m1.ppm`n")
    $s.Write($b, 0, $b.Length)
    Start-Sleep -Milliseconds 1000

    $b = [Text.Encoding]::ASCII.GetBytes("quit`n")
    $s.Write($b, 0, $b.Length)
    Start-Sleep -Seconds 1
    $c.Close()
} finally {
    Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
}

Write-Host "Converting PPM screenshots to PNG..."
C:\Users\ttt79\AppData\Local\Python\bin\python.exe -c "
from PIL import Image
import os
for f in ['build/doom_menu_down', 'build/doom_gameplay', 'build/doom_e1m1']:
    ppm = f + '.ppm'
    png = f + '.png'
    if os.path.exists(ppm):
        Image.open(ppm).save(png)
        print('Saved', png)
"

if (Test-Path build\serial.log) {
    Get-Content build\serial.log -Tail 30
}
