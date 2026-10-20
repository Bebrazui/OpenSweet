# term_cmds.ps1 - boot headless and type commands from a text file into the
# terminal, screenshotting after each one.
#
# Command file format: one command per line. A line starting with "#shot NAME"
# takes a screenshot instead of typing.
param(
    [string]$File = 'scripts\cmds.txt',
    [int]$MonitorPort = 4491,
    [int]$BootWaitSec = 22,
    [int]$WaitPerCmd = 4,
    [string]$Prefix = 'build\cmd',
    [switch]$Leave
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
    Start-Sleep -Milliseconds 100
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
    $i = 0
    foreach ($line in (Get-Content $File)) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        if ($line.StartsWith('#shot ')) {
            $name = $line.Substring(6).Trim()
            Send $s "screendump $name"
            Start-Sleep -Seconds 2
            continue
        }
        if ($line.StartsWith('#wait ')) {
            Start-Sleep -Seconds ([int]$line.Substring(6).Trim())
            continue
        }
        if ($line.StartsWith('#')) { continue }
        Write-Host "> $line"
        SendText $s $line
        Start-Sleep -Seconds $WaitPerCmd
    }
}
finally {
    if (-not $Leave) {
        Start-Sleep -Seconds 1
        $q | Stop-Process -Force -ErrorAction SilentlyContinue
    }
}
