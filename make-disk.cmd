@echo off
rem Opensweet OS - create 16MB test disk with ASCII banner at sector 0
cd /d %~dp0
if not exist build mkdir build
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0make-ext4.ps1"
