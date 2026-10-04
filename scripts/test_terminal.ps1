$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot\..

Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1

$qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$q = Start-Process -FilePath $qemu -ArgumentList `
    "-accel","whpx","-accel","tcg", `
    "-cpu","max", `
    "-m","512M", `
    "-drive","format=raw,file=build\os.img", "-drive","format=raw,file=build\disk.img,if=ide,index=1", `
    "-display","none", `
    "-serial","file:build\serial.log", `
    "-monitor","telnet:127.0.0.1:4444,server,nowait" -PassThru

Start-Sleep 12

try {
    $client = New-Object System.Net.Sockets.TcpClient('127.0.0.1', 4444)
    $stream = $client.GetStream()

    function Send-MonitorCmd($cmd) {
        $bytes = [System.Text.Encoding]::ASCII.GetBytes("$cmd`r`n")
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush()
        Start-Sleep -Milliseconds 200
    }

    Write-Host "Sending echo hello..."
    foreach ($c in 'echo hello'.ToCharArray()) {
        if ($c -eq ' ') { Send-MonitorCmd "sendkey spc" }
        else { Send-MonitorCmd "sendkey $c" }
    }
    Send-MonitorCmd "sendkey ret"
    Start-Sleep 1

    Write-Host "Sending cd /bin..."
    Send-MonitorCmd "sendkey c"
    Send-MonitorCmd "sendkey d"
    Send-MonitorCmd "sendkey spc"
    Send-MonitorCmd "sendkey slash"
    Send-MonitorCmd "sendkey b"
    Send-MonitorCmd "sendkey i"
    Send-MonitorCmd "sendkey n"
    Send-MonitorCmd "sendkey ret"
    Start-Sleep 1

    Write-Host "Typing ls..."
    Send-MonitorCmd "sendkey l"
    Send-MonitorCmd "sendkey s"
    Start-Sleep 1

    Send-MonitorCmd "screendump build/test_results.ppm"
    Start-Sleep 1

    Send-MonitorCmd "quit"
    Start-Sleep 1
    $client.Close()
} finally {
    Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
}

python -c "from PIL import Image; Image.open('build/test_results.ppm').save('build/test_results.png'); print('Converted test_results.png successfully!')"
Write-Host "Complete!"
