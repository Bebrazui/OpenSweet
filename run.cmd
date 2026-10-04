@echo off
rem Opensweet OS - run in QEMU with native SDL Direct3D 11 window, serial -> build\serial.log
setlocal
cd /d %~dp0
call build.cmd
if errorlevel 1 exit /b 1
powershell -ExecutionPolicy Bypass -File make-ext4.ps1
if errorlevel 1 exit /b 1
type nul > build\serial.log
set SDL_RENDER_SCALE_QUALITY=1
set SDL_HINT_RENDER_SCALE_QUALITY=linear
"C:\Program Files\qemu\qemu-system-x86_64.exe" -accel whpx -accel tcg,thread=multi -cpu max -m 512M -drive format=raw,file=build\os.img -drive format=raw,file=build\disk.img,if=ide,index=1 -vga std -global VGA.vgamem_mb=64 -display sdl -serial file:build\serial.log

