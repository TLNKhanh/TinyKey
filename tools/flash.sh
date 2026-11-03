#!/usr/bin/env bash
# Flash a TinyKey firmware image over a WCH-Link.
#
#   ./tools/flash.sh prebuilt/tinykey_v2.bin
#
# Requires minichlink (from ch32v003fun) in PATH and the link in RISC-V debug
# mode (USB 1A86:8010). Always finishes with -b so the chip leaves halt.
set -euo pipefail

if [ $# -lt 1 ]; then
    echo "usage: $0 <firmware.bin>" >&2
    exit 2
fi

bin=$1
[ -f "$bin" ] || { echo "no such file: $bin" >&2; exit 1; }
command -v minichlink >/dev/null 2>&1 || {
    echo "minichlink not found in PATH" >&2
    exit 1
}

# minichlink can hang holding the USB link, so time it out when possible.
if command -v timeout >/dev/null 2>&1; then
    timeout 30 minichlink -w "$bin" flash -b
else
    echo "note: 'timeout' not found, running minichlink without a time limit" >&2
    minichlink -w "$bin" flash -b
fi

echo "flashed $bin"
