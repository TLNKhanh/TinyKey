# TinyKey C firmware - build without make (PowerShell)
#
#   $env:CH32FUN        = path to .../ch32v003fun/ch32v003fun   (optional)
#   $env:RISCV_TOOLCHAIN= directory holding riscv-none-elf-gcc  (optional)
#
# Defaults: ch32v003fun is expected as a sibling of this repo, the toolchain is
# taken from PATH.
$ErrorActionPreference = 'Stop'
$P      = $PSScriptRoot
$Target = 'tinykey_led_test'

# ---- toolchain --------------------------------------------------------------
$TC = $env:RISCV_TOOLCHAIN
if (-not $TC) {
    $gcc = Get-Command 'riscv-none-elf-gcc.exe' -ErrorAction SilentlyContinue
    if ($gcc) { $TC = Split-Path $gcc.Source -Parent }
}
if (-not $TC -or -not (Test-Path (Join-Path $TC 'riscv-none-elf-gcc.exe'))) {
    throw "riscv-none-elf-gcc not found. Add its bin directory to PATH or set `$env:RISCV_TOOLCHAIN."
}

# ---- ch32v003fun framework --------------------------------------------------
$FUN = $env:CH32FUN
if (-not $FUN) { $FUN = Join-Path $P '..\..\..\ch32v003fun\ch32v003fun' }
if (-not (Test-Path (Join-Path $FUN 'ch32v003fun.c'))) {
    throw "ch32v003fun not found at '$FUN'. Clone it as a sibling of this repo or set `$env:CH32FUN."
}

Set-Location $P

$cflags = @(
    '-g','-Os','-flto','-ffunction-sections','-static-libgcc',
    '-march=rv32ec','-mabi=ilp32e',
    "-I$FUN",'-nostdlib','-I.','-Wall'
)
$ldflags = @(
    '-T', (Join-Path $FUN 'ch32v003fun.ld'),
    '-Wl,--gc-sections',
    '-L', (Join-Path $FUN '..\misc'),
    '-lgcc'
)

& (Join-Path $TC 'riscv-none-elf-gcc.exe') -o "$Target.elf" `
    (Join-Path $FUN 'ch32v003fun.c') "$Target.c" @cflags @ldflags
if ($LASTEXITCODE -ne 0) { Write-Host 'COMPILE FAILED'; exit 1 }

& (Join-Path $TC 'riscv-none-elf-size.exe') "$Target.elf"
& (Join-Path $TC 'riscv-none-elf-objcopy.exe') -O binary "$Target.elf" "$Target.bin"
& (Join-Path $TC 'riscv-none-elf-objcopy.exe') -O ihex   "$Target.elf" "$Target.hex"

$hash = (Get-FileHash "$Target.bin" -Algorithm MD5).Hash
Write-Host "BUILD OK -> $Target.bin  ($((Get-Item "$Target.bin").Length) bytes, md5 $hash)"
