# Hardware

TinyKey is a two-layer keychain board around a **CH32V003F4P6** (TSSOP-20),
with a 7x7 LED matrix on the front, a **TP4056** LiPo charger and a USB-C
connector on the back. USB-C is **power only** - D+/D- are not connected to the
MCU, so there is no USB bootloader and no serial port.

The current board (`hardware/TinyKey.kicad_pcb`, often called v2) is about
**35 x 45.4 mm** with a keyring neck and a 5 mm hole; the gerber job size is
35.05 x 45.45 mm.

## Pin map

| Function | Net | MCU pin |
|---|---|---|
| LED anodes A1..A7 | `LED_A1..LED_A7` | PC0, PC1, PC2, PC3, PC4, PC5, PC6 |
| LED cathodes K1..K7 | `LED_K1..LED_K7` | PC7, PD0, PD2, PD3, PD4, PD5, PD6 |
| Button 1 (next effect) | `BTN1` | PD7 / NRST |
| Button 2 (sleep) | `BTN2` | PA1 |
| TP4056 charge status | `CHRG` | PA2 |
| Debug / programming | `SWIO` | PD1 |
| Charge indicator LED | `D50` | - (back side) |

The matrix skips `PD1` on purpose: it is the SWIO programming pin.

LED numbering: `D_n` maps to **row `(n-1)/7`** (anode line `A[row+1]`) and
**column `(n-1)%7`** (cathode line `K[col+1]`).

## LEDs are soldered backwards

All 49 front LEDs are mounted in **reverse polarity**. A lit LED needs
**anode line LOW and cathode line HIGH**. This is handled in firmware
(`POL_REVERSED 1` in the C firmware, inverted drive in the Arduino sketches)
and is the reason the row scan drives the "off" state as high-Z instead of
actively driving the opposite level.

If you build a new board with the LEDs the right way round, flip the polarity
define instead of rewiring.

## No series resistors - read this before changing the GPIO speed

There are **no current-limiting resistors** between the MCU and the matrix.
Peak current is bounded only by the slow **2 MHz** GPIO output setting. Do not
raise the drive strength (10/50 MHz) to make the display brighter: brightness
comes from duty cycle, not drive current, and the pin current has no other
limit.

The display is driven with a **row scan**: one anode line per step, only the
lit cathodes of that row enabled, everything else high-Z. A frame is always
exactly 7 steps, whether a row has lit LEDs or not, so the duty cycle is a
constant 1/7 (~14 %) and the refresh rate stays around 570 Hz at a 250 us
dwell.

Skipping empty rows is a trap: the frame time then follows the picture, and
while text scrolls the panel pumps by roughly 3x at the scroll rate. Both the
C firmware and the current sketch keep the dwell unconditional.

## PD7 is NRST by default

`BTN1` is wired to PD7, which is also `NRST`. From the factory the reset
function is enabled, so pressing `BTN1` **resets the MCU**; earlier firmware
blinked one LED on boot, which looked like "the button blinks an LED instead of
changing effect".

One option byte decides this - `OB->USER`, bits 3-4:

| `USER` bits 3-4 | Meaning |
|---|---|
| `00` | reset IO enabled, ignore delay 128 ms |
| `01` (`0x08`) | reset IO enabled, ignore delay 1 ms |
| `10` (`0x10`) | reset IO enabled, ignore delay 12 ms |
| `11` (`0x18`) | **reset IO disabled -> PD7 is a normal GPIO** |

The prototype shipped with `USER = 0x17` (bits 3-4 = `10`), i.e. reset
enabled. Rewrite the option block to `USER = 0x1F` with complement `0xE0`:

```
# 16 bytes: RDPR, USER, DATA0, DATA1, WRPR0, WRPR1 (value + complement)
a5 5a 1f e0 00 ff 00 ff ff 00 ff 00 ff 00 ff 00
# ^RDPR ^USER
```

```
minichlink -w ob_new.bin option       # 'option' = 0x1FFFF800
minichlink -r /tmp/ob2.bin option 16  # verify
```

Keep `RDPR = 0xA5` (read protection disabled) or you lock yourself out. The
write must not cross a 64-byte boundary; the block above is one aligned write.
`minichlink -D` claims to "configure NRST as GPIO" but does not work through
the programmer - write the option bytes instead.

With NRST disabled the link's reset line no longer resets the chip, but
SWIO/debug reset still works, so flashing and readback are unaffected.

## Charge status

`PA2` is connected to the TP4056 `CHRG` pin (open-drain, active low) and `D50`
on the back is the charge LED. The current sketches do not use it; it is
available if you want the matrix to show charging state.

## PCB revisions

| File | Notes |
|---|---|
| `hardware/TinyKey.kicad_pcb` / `.kicad_pro` | current board (V2, 35 x 45.4 mm), neck + 5 mm hole, ground zones on both layers. **Not manufactured yet.** |
| `hardware/TinyKey.kicad_sch` | schematic |
| `hardware/archive/TinyKey-keychain.*` | first keychain outline, before the neck |
| `hardware/archive/TinyKeyV1.kicad_pcb` | V1, 40 x 52 mm. The board that was actually built and is in the README photos |
| `hardware/archive/TinyKey2.*` | intermediate revision |
| `hardware/gerbers/` | plot of the current board: 9 gerbers, job file, PTH/NPTH drill |
| `hardware/TinyKeyV2_gerbers.zip` | the same plot zipped for a fab house |

Printable exports live in `docs/`: the
[schematic PDF](hardware/TinyKey-schematic.pdf), the
[PCB top](hardware/TinyKey-pcb-top.pdf) and
[PCB bottom](hardware/TinyKey-pcb-bottom.pdf) PDFs, and the
[BOM](../hardware/bom.csv). The BOM lists 50 LEDs (49 matrix + D50), one
CH32V003, one TP4056, the USB-C socket and the passives, with DNP parts
excluded.

DRC on the current board: **0 errors, 0 unconnected**. The remaining warnings
are a library-footprint mismatch on the MCU and USB-C footprints and two
silkscreen texts 0.7 mm tall (minimum is 0.8 mm); neither affects fabrication.
The USB-C shell drills are 0.7 mm, so the board was checked with the local
hole-clearance rule lowered from 0.19 mm to 0.16 mm.

## Prototype notes

- `BTN2` on the development board did not read reliably (it behaved as never
  pressed), so sleep/wake is a bonus feature rather than a requirement. The
  current sketch still polls it.
- The first panel was visibly dim and flickered when scanned one LED at a time
  (1/49 duty). Row scan plus PWM fixed it; see `bringup/` for the sketches that
  proved the wiring.
