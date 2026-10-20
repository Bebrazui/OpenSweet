param([int]$Runs = 2, [int]$UnpackWaitSec = 180)
$ErrorActionPreference = 'Continue'
Set-Location (Join-Path $PSScriptRoot '..')
for ($i = 1; $i -le $Runs; $i++) {
    Write-Host "===== RUN $i =====" -ForegroundColor Magenta
    & powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test_bash.ps1 -RebuildDisk -SerialTail 12 -UnpackWaitSec $UnpackWaitSec 2>&1 |
        Select-String -Pattern 'Step |SMOKE|wild|extents full|ata retry|\[io\]|Loading executable|loaded segments|No valid PT_LOAD|Unhandled|Exit with code' |
        ForEach-Object { $_.Line }
    Copy-Item build\serial.log "build\serial_run$i.log" -ErrorAction SilentlyContinue
    Copy-Item build\bash_ps_result.ppm "build\bash_ps_run$i.ppm" -ErrorAction SilentlyContinue
}
Write-Host "===== SUMMARY =====" -ForegroundColor Magenta
Get-ChildItem build\serial_run*.log | Sort-Object LastWriteTime | Select-Object -Last $Runs | ForEach-Object {
    $p = $_.FullName
    $bad = Select-String -Path $p -Pattern 'extents full|!blk wild|\[io\]'
    $bash = Select-String -Path $p -Pattern "Loading executable: '/bin/bash'"
    $ok = Select-String -Path $p -Pattern 'GNU bash, version'
    "{0}: bad={1} bashed={2}" -f $_.Name, ($(if ($bad) { $bad.Count } else { 0 })), ($(if ($bash) { 'yes' } else { 'no' }))
}
Write-Host "===== DONE =====" -ForegroundColor Magenta
