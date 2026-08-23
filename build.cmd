@echo off
setlocal
set FASM=C:\Users\ttt79\Downloads\fasmw17335\FASM.EXE
cd /d %~dp0
if not exist build mkdir build

rem close any running QEMU - it locks build\os.img
taskkill /IM qemu-system-x86_64.exe /F >nul 2>&1
timeout /t 1 /nobreak >nul

%FASM% arch\x86_64\boot\boot.asm build\boot.bin
if errorlevel 1 exit /b 1
%FASM% arch\x86_64\boot\stage2.asm build\stage2.bin
if errorlevel 1 exit /b 1
%FASM% arch\x86_64\kernel\kernel.asm build\kernel.bin
if errorlevel 1 exit /b 1

del build\os.img 2>nul
powershell -NoProfile -Command "$m=[IO.File]::ReadAllBytes('build\boot.bin');$s=[IO.File]::ReadAllBytes('build\stage2.bin');$k=[IO.File]::ReadAllBytes('build\kernel.bin');$s2=New-Object byte[] 16384;[Array]::Copy($s,$s2,$s.Length);$o=New-Object byte[] 1474560;[Array]::Copy($m,0,$o,0,$m.Length);[Array]::Copy($s2,0,$o,512,$s2.Length);[Array]::Copy($k,0,$o,512+16384,$k.Length);[IO.File]::WriteAllBytes('build\os.img',$o)"
if errorlevel 1 (
    echo BUILD FAILED: image assembly
    exit /b 1
)

echo Build OK: build\os.img ^(MBR + stage2 16KB + kernel^)
