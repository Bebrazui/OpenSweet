@echo off
rem Opensweet OS - automated keyboard test: test.cmd [keys...] (default: d i v ret)
setlocal
cd /d %~dp0
set "KEYS=%*"
if "%~1"=="" set "KEYS=d i v ret"
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-keys.ps1 -KeysCsv "%KEYS%"
