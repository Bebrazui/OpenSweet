@echo off
rem Opensweet OS - automated keyboard test: test.cmd [keys...] (default: d i v ret)
setlocal
cd /d %~dp0
if "%~1"=="" (
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-keys.ps1
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-keys.ps1 -Keys %*
)
