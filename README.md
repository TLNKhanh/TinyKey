# TinyKey

A keychain with 49 LEDs on the front and a CH32V003 doing the work.

This started as a "can I build a display small enough to actually carry" project.
The CH32V003F4P6 turned out to be a good fit: 16 kB flash, 2 kB RAM, a 48 MHz
RISC-V core, and it happily runs from a small LiPo. So the board got a 7x7
matrix on one side, a TP4056 charger and a USB-C socket on the other, and a
5 mm hole in the neck for the keyring.

![TinyKey board, front side](docs/images/board-hero.jpg)

## The hardware

The front is 49 0805 LEDs wired as 7 anode rows and 7 cathode columns - the
whole matrix runs on 14 GPIOs. The back carries the MCU, the charger, a USB-C
socket (power only; the data pins go nowhere), two buttons, SWIO pads for
programming and B+/B- pads for the battery. The board is about 35 x 45 mm,
two layers.

Two details are worth knowing before building one:

- The first batch came back with all 49 LEDs soldered backwards. Instead of
  reworking 49 parts I flipped the drive polarity in firmware: a lit LED is
  anode LOW, cathode HIGH. That is still how it works.
- There are no series resistors on the matrix. Current is limited by keeping
  the GPIO drive at 2 MHz. Raising it to get a brighter display is how you cook
  the pins.

There is one more trap. BTN1 is on PD7, which is NRST from the factory, so the
option bytes have to disable the reset function (`USER = 0x1F`) or the button
reboots the chip instead of changing effect. The exact bytes and commands are
in [docs/hardware.md](docs/hardware.md#pd7-is-nrst-by-default).

| Front (49 LEDs) | Back (MCU, charger, buttons) |
|---|---|
| ![PCB front](docs/images/board-top.png) | ![PCB back](docs/images/board-bottom.png) |

Everything you need to build or order one:

- [Schematic PDF](docs/hardware/TinyKey-schematic.pdf) and
  [PNG preview](docs/images/schematic.png)
- [PCB top PDF](docs/hardware/TinyKey-pcb-top.pdf) and
  [PCB bottom PDF](docs/hardware/TinyKey-pcb-bottom.pdf)
- [Gerbers](hardware/gerbers), [fab zip](hardware/TinyKeyV2_gerbers.zip),
  [BOM](hardware/bom.csv)
- KiCad project: [hardware/](hardware/)

The pictures above are renders generated from the board file, so they cannot
go stale. Real photos will replace them once I have a finished unit; the shot
list is in [docs/photos.md](docs/photos.md).

## The software

There are two firmwares in the repo.

[`firmware/arduino/tinykey_v2`](firmware/arduino/tinykey_v2) is the current
sketch. It uses the WCH Arduino core and does 15-step software PWM per LED,
which gives fades and sub-pixel text scrolling. Eight effects: text scroll,
wave, ripple, rain, heart, bounce, chevron, and a small face that blinks -
because a 7x7 display deserves a face.

[`firmware/c`](firmware/c) is the smaller ch32v003fun version: one C file,
14 effects, no Arduino layer. It is the firmware that first ran on the board
and is a decent starting point if you want to stay close to the metal.

Both use a row scan: one anode line at a time, only that row's lit cathodes
on, seven steps per frame. A frame always takes seven steps whether a row is
empty or not - skipping empty rows makes the brightness pump while text
scrolls. Each LED gets 1/7 duty, the frame runs around 570 Hz, and that is why
the dwell in the renderer is unconditional.

Controls: BTN1 cycles effects, BTN2 sleeps and wakes. BTN2 was unreliable on my
prototype, so treat sleep as a bonus.

### Build and flash

There is no USB bootloader. Programming goes through SWIO (PD1) with a
WCH-Link. Short version:

```bash
# Arduino firmware
arduino-cli compile --fqbn WCH:ch32v:CH32V00x_EVT:pnum=CH32V003F4 \
  --output-dir out firmware/arduino/tinykey_v2
timeout 30 minichlink -w out/tinykey_v2.ino.bin flash -b

# C firmware (needs riscv-none-elf-gcc, ch32v003fun cloned as a sibling)
cd firmware/c
make
make flash
```

Or flash one of the [prebuilt binaries](prebuilt/). Windows WinUSB setup, the
option-byte write and the usual minichlink problems are covered in
[docs/flashing.md](docs/flashing.md).

## Repository layout

| Path | Contents |
|---|---|
| `firmware/arduino/tinykey_v2/` | current firmware, WCH Arduino core |
| `firmware/arduino/legacy/` | earlier sketches, kept for reference |
| `firmware/c/` | ch32v003fun firmware, 14 effects |
| `bringup/` | six small sketches used to bring the first board up |
| `hardware/` | KiCad project, gerbers, BOM, fab zip |
| `docs/` | pin map, option bytes, flashing, photo notes |
| `host/` | Windows helper to bind WinUSB to the WCH-Link |
| `tools/` | `flash.sh` and `verify.sh` for Linux/macOS |
| `prebuilt/` | ready-to-flash binaries and checksums |

## Status

Working prototype. The C firmware is the build that ran on the first board;
the Arduino sketch is the newer rewrite and compiles to 8932 B of 16 kB. The
prebuilt binaries come from the sources here, but flash and check them on your
own board - they were built, not blessed.

## License

MIT - see [LICENSE](LICENSE).
