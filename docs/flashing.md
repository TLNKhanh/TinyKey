# Flashing

The CH32V003 has no USB bootloader on this board (USB-C data pins are not
connected). Programming goes through **SWIO on PD1** with a **WCH-Link** or
WCH-LinkE. The proven tool is
[minichlink](https://github.com/cnlohr/ch32v003fun/tree/master/minichlink)
from ch32v003fun; OpenOCD and the Arduino core's uploader are less reliable,
especially on Windows.

## Wiring

| WCH-Link | TinyKey |
|---|---|
| 3V3 | 3V3 |
| GND | GND |
| SWIO | PD1 |

The WCH-Link must enumerate as **USB `1A86:8010`** (RISC-V debug mode). If it
shows up as `1A86:E317` it is in HID / firmware-upgrade mode and neither
minichlink nor OpenOCD will see it; unplug and replug it (do not hold the
button while plugging in).

## Linux / macOS

Build or install `minichlink` from
[ch32v003fun](https://github.com/cnlohr/ch32v003fun) and install its udev
rules so the link is writable without root. Then:

```bash
# flash
timeout 30 minichlink -w prebuilt/tinykey_v2.bin flash -b

# verify: read the whole 16 kB image back and compare
timeout 30 minichlink -r /tmp/readback.bin 0x08000000 16384
head -c "$(stat -c%s prebuilt/tinykey_v2.bin)" /tmp/readback.bin | cmp - prebuilt/tinykey_v2.bin
```

`tools/flash.sh` and `tools/verify.sh` wrap those two commands.

Notes:

- **Always finish with `-b`.** A bare readback leaves the chip halted and the
  display frozen until the next reset.
- **Wrap minichlink in `timeout`.** It can hang holding the USB link if another
  process already claimed it.
- `not in plugdev/dialout` warnings can be ignored once the udev rules are
  installed.

## Windows

minichlink uses libusb, so the WCH-Link debug interface needs **WinUSB** bound
to it (`USB\VID_1A86&PID_8010&MI_00`). Two ways:

1. **Zadig** - select the interface and install WinUSB.
2. **Scripted, no WDK**: `host/install_wchlink_winusb.ps1` (elevated) creates
   a self-signed code-signing certificate, signs a minimal WinUSB INF
   (`host/wchlink_winusb.inf`) and installs it with `pnputil`. Because the INF
   only binds the inbox `winusb.sys`, Windows accepts the self-signed catalog.

```powershell
# elevated PowerShell
powershell -ExecutionPolicy Bypass -File host\install_wchlink_winusb.ps1
```

Common problems:

- `libusb_open() = -3` (access denied): a previous minichlink still holds the
  link. `taskkill /F /IM minichlink.exe`, then retry.
- `libusb_open() = -5`: the interface is bound to the WCH vendor driver instead
  of WinUSB. Re-run the WinUSB installer (or use Zadig).
- Old ch32v003fun builds hang after printing the chip info; use a current
  `minichlink.exe` and keep `libusb-1.0.dll` next to it.
- The bundled OpenOCD in the WCH Arduino core can segfault at
  `wch_riscv_init_target`. Compile with `arduino-cli`, then flash the produced
  `.bin` with minichlink - that path is reliable.

## Arduino sketches

The WCH core compiles for `CH32V00x_EVT` with `pnum=CH32V003F4`:

```bash
arduino-cli compile --fqbn WCH:ch32v:CH32V00x_EVT:pnum=CH32V003F4 \
  --output-dir out firmware/arduino/tinykey_v2
timeout 30 minichlink -w out/tinykey_v2.ino.bin flash -b
```

The `.bin` that the core produces starts at `0x08000000`, which is where
minichlink's `flash` target expects it.

## Option bytes (only once per chip)

`BTN1` sits on PD7/NRST, so the reset function must be disabled or the button
reboots the MCU. See
[docs/hardware.md](hardware.md#pd7-is-nrst-by-default) for the full table and
the 16-byte block. Short version:

```
minichlink -w ob_new.bin option
minichlink -r /tmp/ob2.bin option 16
```

Keep `RDPR = 0xA5` (read protection off). Losing it locks you out of the chip.

## If the display freezes after flashing

The chip is probably halted: run one more `minichlink -r ... -b` (or flash
again with `-b`) to resume. This is normal behaviour, not a firmware bug.
