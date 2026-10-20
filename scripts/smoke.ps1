# Opensweet OS - headless smoke test.
#
# Boots the OS with no display, captures the COM1 serial log, and asserts on its
# contents. Serial output is the only trustworthy signal: screenshots need a human
# and tell you nothing in CI.
#
# Exit code 0 = all required markers seen and no fatal markers present.
# Exit code 1 = a required marker is missing, or a fatal marker was found.
#
# Examples:
#   powershell -File scripts/smoke.ps1                # boot whatever is in build/
#   powershell -File scripts/smoke.ps1 -Build         # rebuild first
#   powershell -File scripts/smoke.ps1 -BootWaitSec 40
#
# Environment:
#   OPENSWEET_QEMU  - path to qemu-system-x86_64.exe (default: standard install path)

param(
    [switch]$Build,
    [int]$BootWaitSec = 25,
    [string]$QemuPath = $(if ($env:OPENSWEET_QEMU) { $env:OPENSWEET_QEMU } else { 'C:\Program Files\qemu\qemu-system-x86_64.exe' }),
    # Markers that must all appear in the serial log.
    [string[]]$Require = @(
        'ext4_ok=01',
        'System initialized successfully',
        'Ring 3 process spawned'
    ),
    # Markers that must not appear. Matching any of these fails the run.
    [string[]]$Forbid = @(
        'EXCEPTION',
        'Page Fault',
        'GENERAL PROTECTION',
        'Double Fault',
        'PANIC'
    ),
    # Markers worth reporting but not failing on (the Linux ABI layer is still WIP).
    [string[]]$Warn = @(
        'Unhandled syscall'
    ),
    [string]$SerialLog = 'build\serial_smoke.log'
)

$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')

function Fail([string]$msg) {
    Write-Host "SMOKE FAIL: $msg" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $QemuPath)) {
    Fail "qemu not found at '$QemuPath' (set OPENSWEET_QEMU or pass -QemuPath)"
}

if ($Build) {
    Write-Host 'Smoke: rebuilding os.img and disk.img...' -ForegroundColor Cyan
    & cmd /c build.cmd
    if ($LASTEXITCODE -ne 0) { Fail 'build.cmd failed' }
    & powershell -NoProfile -ExecutionPolicy Bypass -File make-ext4.ps1
    if ($LASTEXITCODE -ne 0) { Fail 'make-ext4.ps1 failed' }
}

foreach ($img in @('build\os.img', 'build\disk.img')) {
    if (-not (Test-Path $img)) { Fail "$img missing - run build.cmd / make-ext4.ps1 or pass -Build" }
}

Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item $SerialLog -ErrorAction SilentlyContinue

Write-Host "Smoke: booting headless for ${BootWaitSec}s..." -ForegroundColor Cyan
$q = Start-Process -WindowStyle Hidden -FilePath $QemuPath -ArgumentList `
    '-accel', 'whpx', '-accel', 'tcg', `
    '-cpu', 'max', '-m', '512M', `
    '-drive', 'format=raw,file=build\os.img', `
    '-drive', 'format=raw,file=build\disk.img,if=ide,index=1', `
    '-vga', 'std', '-global', 'VGA.vgamem_mb=64', `
    '-display', 'none', `
    '-serial', "file:$SerialLog", `
    '-no-reboot' -PassThru

Start-Sleep -Seconds $BootWaitSec
$exitedEarly = $q.HasExited
Get-Process qemu-system-* -ErrorAction SilentlyContinue | Stop-Process -Force

if (-not (Test-Path $SerialLog)) { Fail 'no serial log produced - QEMU did not start correctly' }
$log = Get-Content $SerialLog -Raw
if ([string]::IsNullOrWhiteSpace($log)) { Fail 'serial log is empty - kernel produced no output' }

Write-Host "Smoke: captured $($log.Length) bytes of serial output." -ForegroundColor Cyan

$problems = @()
foreach ($m in $Require) {
    if ($log -notmatch [regex]::Escape($m)) { $problems += "missing required marker: '$m'" }
}
foreach ($m in $Forbid) {
    if ($log -match [regex]::Escape($m)) { $problems += "fatal marker present: '$m'" }
}
if ($exitedEarly) { $problems += 'QEMU exited before the wait elapsed (kernel likely reset or crashed)' }

foreach ($m in $Warn) {
    $hits = ([regex]::Matches($log, [regex]::Escape($m))).Count
    if ($hits -gt 0) { Write-Host "Smoke: warning - '$m' seen $hits time(s)." -ForegroundColor Yellow }
}

if ($problems.Count -gt 0) {
    Write-Host '--- serial log ---' -ForegroundColor DarkGray
    Write-Host $log
    Fail ($problems -join '; ')
}

Write-Host 'SMOKE OK' -ForegroundColor Green
exit 0
