# TinyKey C firmware - flash over WCH-Link (PowerShell)
#
#   $env:MINICHLINK = path to minichlink.exe (optional)
#
# Requires the WCH-Link wired to SWIO (PD1), GND and 3V3, and WinUSB bound to
# its debug interface on Windows (see docs/flashing.md).
$ErrorActionPreference = 'Stop'
$P      = $PSScriptRoot
$Target = 'tinykey_led_test'

$BIN = Join-Path $P "$Target.bin"
if (-not (Test-Path $BIN)) { throw "$BIN not found - run build.ps1 first." }

$MINICHLINK = $env:MINICHLINK
if (-not $MINICHLINK) {
    $cmd = Get-Command 'minichlink.exe' -ErrorAction SilentlyContinue
    if ($cmd) { $MINICHLINK = $cmd.Source }
}
if (-not $MINICHLINK) {
    throw 'minichlink.exe not found. Put it on PATH or set $env:MINICHLINK.'
}

# a stale instance may still hold the USB link
Get-Process minichlink -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

& $MINICHLINK -w $BIN flash -b
if ($LASTEXITCODE -ne 0) { Write-Host "FLASH FAILED (exit $LASTEXITCODE)"; exit 1 }
Write-Host 'FLASH OK'
