@echo off
rem Opensweet OS - create 16MB test disk with ASCII banner at sector 0
cd /d %~dp0
if not exist build mkdir build
powershell -NoProfile -Command "$b = New-Object byte[] 16777216; $s = [Text.Encoding]::ASCII.GetBytes('OPENSWEET ATA DISK OK'); [Array]::Copy($s, 0, $b, 0, $s.Length); $t = [Text.Encoding]::ASCII.GetBytes('SECTOR1'); [Array]::Copy($t, 0, $b, 512, $t.Length); [IO.File]::WriteAllBytes('build\disk.img', $b)"
echo Build OK: build\disk.img
