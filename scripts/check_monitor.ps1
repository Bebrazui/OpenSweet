$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot\..

Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1

$qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$q = Start-Process -FilePath $qemu -ArgumentList `
    "-m","512M", `
    "-drive","format=raw,file=build\os.img", "-drive","format=raw,file=build\disk.img,if=ide,index=1", `
    "-display","none", `
    "-serial","file:build\serial.log", `
    "-monitor","telnet:127.0.0.1:4444,server,nowait" -PassThru

Start-Sleep 6

try {
    $client = New-Object System.Net.Sockets.TcpClient('127.0.0.1', 4444)
    $stream = $client.GetStream()
    $reader = New-Object System.IO.StreamReader($stream, [System.Text.Encoding]::ASCII)
    $writer = New-Object System.IO.StreamWriter($stream, [System.Text.Encoding]::ASCII)
    $writer.AutoFlush = $true

    Start-Sleep 1
    # Read initial telnet banner
    while ($stream.DataAvailable) {
        Write-Host "QEMU: $($reader.ReadLine())"
    }

    $writer.WriteLine("sendkey t")
    Start-Sleep -Milliseconds 500
    while ($stream.DataAvailable) {
        Write-Host "After sendkey t: $($reader.ReadLine())"
    }

    $writer.WriteLine("quit")
    Start-Sleep 1
    $client.Close()
} finally {
    Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
}
