@echo off
setlocal
cd /d %~dp0
if not exist build mkdir build

rem Toolchain path. Override from the environment before running:
rem     set FASM=C:\tools\fasm\FASM.EXE
if not defined FASM set "FASM=C:\Users\ttt79\Downloads\fasmw17335\FASM.EXE"
if not exist "%FASM%" (
    echo BUILD ERROR: FASM not found at "%FASM%"
    echo Set the FASM environment variable to your FASM.EXE path, e.g.
    echo     set FASM=C:\tools\fasm\FASM.EXE
    exit /b 1
)

rem close any running QEMU - it locks build\os.img
taskkill /IM qemu-system-x86_64.exe /F >nul 2>&1
timeout /t 1 /nobreak >nul

rem Build the kernel first: stage2 must know its exact size so that no tail
rem of the image is ever left unloaded.
"%FASM%" arch\x86_64\kernel\kernel.asm build\kernel.bin
if errorlevel 1 exit /b 1

rem The kernel is loaded at 0x10000 and must stay below the EBDA (~0x9F000).
for %%A in (build\kernel.bin) do set KSIZE=%%~zA
if %KSIZE% GTR 585728 (
    echo BUILD ERROR: kernel.bin is %KSIZE% bytes, limit is 585728 ^(0x8F000^)
    exit /b 1
)
set /a KSEC=(KSIZE + 511) / 512

"%FASM%" -d KERNEL_SECTORS=%KSEC% arch\x86_64\boot\stage2.asm build\stage2.bin
if errorlevel 1 exit /b 1
"%FASM%" arch\x86_64\boot\boot.asm build\boot.bin
if errorlevel 1 exit /b 1

del build\os.img 2>nul
powershell -NoProfile -Command "$size=16*1024*1024;$m=[IO.File]::ReadAllBytes('build\boot.bin');$s=[IO.File]::ReadAllBytes('build\stage2.bin');$k=[IO.File]::ReadAllBytes('build\kernel.bin');$s2=New-Object byte[] 16384;[Array]::Copy($s,$s2,$s.Length);$o=New-Object byte[] $size;[Array]::Copy($m,0,$o,0,$m.Length);[Array]::Copy($s2,0,$o,512,$s2.Length);[Array]::Copy($k,0,$o,512+16384,$k.Length);[IO.File]::WriteAllBytes('build\os.img',$o)"
if errorlevel 1 (
    echo BUILD FAILED: image assembly
    exit /b 1
)

echo Build OK: build\os.img ^(16MB raw disk: MBR + stage2 16KB + kernel^)
