@echo off
rem Opensweet OS - show last kernel serial log
cd /d %~dp0
if not exist build\serial.log (
    echo No build\serial.log yet
    exit /b 1
)
type build\serial.log
