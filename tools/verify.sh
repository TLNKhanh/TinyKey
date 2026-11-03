#!/usr/bin/env bash
# Read the whole 16 kB flash back and compare it with a local image.
#
#   ./tools/verify.sh prebuilt/tinykey_v2.bin
#
# A bare readback leaves the chip halted (the display freezes), so this always
# reboots out of halt afterwards.
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

tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

run() {
    if command -v timeout >/dev/null 2>&1; then
        timeout 30 "$@"
    else
        "$@"
    fi
}

run minichlink -r "$tmp" 0x08000000 16384
run minichlink -b   # resume; never leave the chip halted

size=$(wc -c < "$bin")
if head -c "$size" "$tmp" | cmp -s - "$bin"; then
    echo "OK: first $size bytes match $bin"
else
    echo "MISMATCH: flash contents differ from $bin" >&2
    exit 1
fi
