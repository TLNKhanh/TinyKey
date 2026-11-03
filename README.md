# TinyKey

A keychain with 49 LEDs on the front and a ten cent MCU doing the work.

<p align="center"><img src="docs/images/photo-front.jpg" width="560" alt="TinyKey keychain, front side lit"></p>

I built this because I wanted to find out how much display you can get out of a
microcontroller that costs less than a coffee. The answer, it turns out, is a
49 LED matrix you can scroll text on, carry in your pocket, and charge from
USB-C. The whole thing runs on a CH32V003, the chip WCH sells for about ten
cents.

## Why a ten cent MCU

The CH32V003F4P6 costs about ten cents and it is a real 32-bit part: RISC-V at
48 MHz, 16 kB flash, 2 kB RAM, 18 GPIOs, UART, I2C, SPI, a 10-bit ADC, all in a
TSSOP-20. No USB, no crystal, no external memory, no display controller.

That is the whole appeal. A 7x7 matrix needs 14 pins and 25 bytes of frame
buffer, and the scan has to keep a fixed frame time or the brightness wobbles.
The chip has just enough of everything, so the money goes into the display
instead of the silicon: no driver IC, no series resistors, and the current limit
is the 2 MHz GPIO drive setting. What you pay in return is software, and for a
keychain that is a good trade.

## V1 or V2?

There are two revisions and they are easy to mix up. The photos are V1, the
files in `hardware/` are V2, and V2 has never been manufactured.

| | V1 | V2 |
|---|---|---|
| Shown in | the photos below | the 3D renders and PDFs |
| Board file | `hardware/archive/TinyKeyV1.kicad_pcb` | `hardware/TinyKey.kicad_pcb` |
| Built | yes, it runs | no, files only |
| Size | 40 x 52 mm | 35 x 45.4 mm |
| What changed | the prototype | tighter outline, ground pours redone, DRC clean |

If you want to order a board, use V2. The photos are just what the thing looks
like assembled, with a cell and a keyring on it.

## The hardware

Front: 49 0805 LEDs wired as 7 anode rows and 7 cathode columns, 14 GPIOs in
total. Back: the MCU, the charger, USB-C (power only, the data pins go nowhere),
two buttons, SWIO pads for programming and B+/B- for the cell. Two layers, no
series resistors.

One thing that bit me: the first batch came back with all 49 LEDs soldered
backwards. Instead of reworking 49 parts I flipped the drive polarity in
firmware, so a lit LED is anode LOW, cathode HIGH. It still works that way.

One trap: BTN1 sits on PD7, which is NRST from the factory. Write the option
bytes (`USER = 0x1F`) or the button reboots the chip instead of changing effect.
The bytes and commands are in
[docs/hardware.md](docs/hardware.md#pd7-is-nrst-by-default).

| Front, 49 LEDs lit | Back, cell taped over the charger |
|---|---|
| ![TinyKey front](docs/images/photo-front.jpg) | ![TinyKey back](docs/images/photo-back.jpg) |

Everything for ordering: [schematic PDF](docs/hardware/TinyKey-schematic.pdf),
[PCB top](docs/hardware/TinyKey-pcb-top.pdf) and
[bottom](docs/hardware/TinyKey-pcb-bottom.pdf) PDFs,
[gerbers](hardware/gerbers), [fab zip](hardware/TinyKeyV2_gerbers.zip),
[BOM](hardware/bom.csv), KiCad [project](hardware/).

## The software

`firmware/arduino/tinykey_v2` is the current sketch, on the WCH Arduino core. It
scrolls "Hello World" and then nine generated patterns, with 15 step software
PWM per LED so text fades and moves in sub-pixel steps.

`firmware/c` is the smaller ch32v003fun build. One C file, 14 effects, no
Arduino layer, and the one that first ran on the board.

BTN1 steps through the patterns. BTN2 sleeps: the sketch blanks the matrix,
stops the tick and halts the core with `WFI`, and the next press wakes it. There
is no load switch on the board, so that is all the power management there is.

### Build and flash

```bash
# Arduino firmware
arduino-cli compile --fqbn WCH:ch32v:CH32V00x_EVT:pnum=CH32V003F4 \
  --output-dir out firmware/arduino/tinykey_v2
timeout 30 minichlink -w out/tinykey_v2.ino.bin flash -b

# C firmware (riscv-none-elf-gcc, ch32v003fun as a sibling directory)
cd firmware/c
make
make flash
```

There is no USB bootloader, so SWIO and a WCH-Link are the only way in. Prebuilt
binaries are in [prebuilt/](prebuilt/), and the Windows driver setup plus the
option bytes are covered in [docs/flashing.md](docs/flashing.md).

## Repo layout

`firmware/arduino/tinykey_v2/` is the current sketch and `firmware/c/` the
ch32v003fun one. `bringup/` and `firmware/arduino/legacy/` are history.
`hardware/` holds the KiCad project, gerbers, BOM and archived revisions,
`docs/` the pin map, flashing and photo notes, `host/` and `tools/` the WinUSB
helper and flash scripts, `prebuilt/` the binaries.

## Status

V1 works. V2 is files and gerbers, not silicon. The Arduino sketch compiles to
9172 B of 16 kB. The prebuilt binaries came from these sources, but I have not
tried them on anyone else's board.

MIT, see [LICENSE](LICENSE).
