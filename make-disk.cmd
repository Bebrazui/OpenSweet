@echo off
rem Opensweet OS - create 16MB test disk with wallpaper and ELF applications
cd /d %~dp0
if not exist build mkdir build
if exist "C:\Users\ttt79\Downloads\opensweet_walp.jpeg" (
    python tools\convert_wallpaper.py "C:\Users\ttt79\Downloads\opensweet_walp.jpeg" "build\wallpaper.png"
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0make-ext4.ps1"
