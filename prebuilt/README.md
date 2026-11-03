# Prebuilt binaries

Compiled from the sources in this repository, ready for `minichlink`.

| File | Size | MD5 | Source |
|---|---|---|---|
| `tinykey_v2.bin` | 9340 B | `C83D892199EDFCB3B2D00D00E97D689B` | `firmware/arduino/tinykey_v2` |
| `tinykey_led_test.bin` | 3356 B | `35822FCC4074547E3D8B2564944F81FF` | `firmware/c` (after the constant-dwell fix) |

```bash
timeout 30 minichlink -w prebuilt/tinykey_v2.bin flash -b
```

`tinykey_v2.bin` rebuilds byte-for-byte with arduino-cli 1.5.1, WCH:ch32v core
1.0.4 and the FQBN in the README. `tinykey_led_test.bin` needs the xPack
RISC-V toolchain and ch32v003fun listed in `firmware/c/README.md`.

Both binaries were built but **not** verified on hardware in this snapshot;
earlier builds of the same sources were flashed and read back successfully.
Flash and check before trusting them.

The C firmware needs the PD7/NRST option-byte fix, and both firmwares expect
the reversed LED polarity of the prototype. See `docs/hardware.md`.
