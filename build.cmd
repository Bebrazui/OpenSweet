@echo off
setlocal
set FASM=C:\Users\ttt79\Downloads\fasmw17335\FASM.EXE
cd /d %~dp0
if not exist build mkdir build

%FASM% arch\x86_64\boot\boot.asm build\boot.bin
if errorlevel 1 exit /b 1
%FASM% arch\x86_64\kernel\kernel.asm build\kernel.bin
if errorlevel 1 exit /b 1

copy /b build\boot.bin+build\kernel.bin build\os.img >nul

rem pad to 1.44MB floppy size (dd-friendly)
powershell -NoProfile -Command "$b=[IO.File]::ReadAllBytes('build\os.img');$o=New-Object byte[] 1474560;[Array]::Copy($b,$o,$b.Length);[IO.File]::WriteAllBytes('build\os.img',$o)"

echo Build OK: build\os.img
