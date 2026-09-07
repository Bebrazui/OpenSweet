@echo off
rem Opensweet OS - run in QEMU with GTK window, serial -> build\serial.log
setlocal
cd /d %~dp0
if not exist build\os.img (
    echo No build\os.img - run build.cmd first
    exit /b 1
)
type nul > build\serial.log
"C:\Program Files\qemu\qemu-system-x86_64.exe" -drive format=raw,file=build\os.img -drive format=raw,file=build\disk.img,if=ide,index=1 -vga std -display gtk -serial file:build\serial.log
