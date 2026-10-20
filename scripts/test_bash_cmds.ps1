# test_bash_cmds.ps1 - drive real Debian bash with pipes, redirects, globs, loops
#
# Boots headless, extracts bash.deb, starts /bin/bash and types a script of
# shell commands through the QEMU monitor, screenshotting the result.
param(
    [int]$MonitorPort = 4488,
    [int]$BootWaitSec = 25,
    [int]$UnpackWaitSec = 60,
    [int]$StepWaitSec = 5,
    [string]$Shot = 'build\bash_cmds.ppm'
)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

$QemuPath = 'C:\Program Files\qemu\qemu-system-x86_64.exe'

$KeyMap = @{
    ' ' = 'spc'; '/' = 'slash'; '.' = 'dot'; '-' = 'minus'; '=' = 'equal'
    "'" = 'apostrophe'; ';' = 'semicolon'; ',' = 'comma'; '\' = 'backslash'
    '[' = 'bracket_left'; ']' = 'bracket_right'; '`' = 'grave_accent'
    '>' = 'shift-dot'; '<' = 'shift-comma'; '|' = 'shift-backslash'
    '$' = 'shift-4'; '%' = 'shift-5'; '*' = 'shift-8'; '(' = 'shift-9'
    ')' = 'shift-0'; ':' = 'shift-semicolon'; '"' = 'shift-apostrophe'
    '!' = 'shift-1'; '&' = 'shift-7'; '#' = 'shift-3'; '@' = 'shift-2'
    '^' = 'shift-6'; '_' = 'shift-minus'; '+' = 'shift-equal'
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

function Shot([System.Net.Sockets.NetworkStream]$s, [string]$name) {
    Send $s "screendump $name"
    Start-Sleep -Seconds 2
}

Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
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

try {
    SendText $s 'dpkg-deb -x /bash.deb /'
    Start-Sleep -Seconds $UnpackWaitSec

    SendText $s '/bin/bash'
    Start-Sleep -Seconds ($StepWaitSec + 2)

    SendText $s 'echo hello-world'
    Start-Sleep -Seconds 2
    SendText $s 'X=42; echo val=$X'
    Start-Sleep -Seconds 2
    SendText $s 'for i in 1 2 3; do echo n$i; done'
    Start-Sleep -Seconds 2
    SendText $s 'echo piped | cat'
    Start-Sleep -Seconds 2
    SendText $s 'echo redir > /tmp/t.txt; cat /tmp/t.txt'
    Start-Sleep -Seconds 2
    SendText $s 'ls / | head -3'
    Start-Sleep -Seconds 3
    SendText $s 'echo abc | wc -c'
    Start-Sleep -Seconds 2
    Shot $s $Shot
}
finally {
    Start-Sleep -Seconds 1
    $q | Stop-Process -Force -ErrorAction SilentlyContinue
}
Write-Host "Screenshot: $Shot"
