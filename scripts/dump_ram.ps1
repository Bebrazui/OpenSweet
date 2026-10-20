# dump_ram.ps1 - boot headless, type a command, then snapshot low guest RAM
param([int]$BootWaitSec = 30, [int]$MonitorPort = 4452, [string]$Text = '/bin/cat', [string]$Text2 = 'hi', [int]$Wait1 = 10, [int]$Wait2 = 6, [string]$Out = 'build\ram.bin', [long]$Base = 0x40000, [long]$Len = 0x40000)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
$QemuPath = 'C:\Program Files\qemu\qemu-system-x86_64.exe'
Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
Remove-Item build\serial.log -ErrorAction SilentlyContinue
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
if ($Text)  { SendText $Text }
Start-Sleep -Seconds $Wait1
if ($Text2) { SendText $Text2 }
Start-Sleep -Seconds $Wait2
Send 'stop'
Start-Sleep -Milliseconds 500
Send "pmemsave $Base $Len $Out"
Start-Sleep -Seconds 8
try { Send 'quit' } catch {}
Start-Sleep -Seconds 1
try { $c.Close() } catch {}
Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Host "saved $Out"
Get-Content build\serial.log -Tail 8
