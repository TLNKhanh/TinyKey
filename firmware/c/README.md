# C firmware (ch32v003fun)

A smaller, self-contained version of the firmware in a single C file:
14 effects, 5x7 font, two buttons, no Arduino core.

- `tinykey_led_test.c` - everything (pin map, font, effects, renderer)
- `funconfig.h` - selects the CH32V003
- `Makefile`, `build.ps1`, `flash.ps1`

## Build

Needs `riscv-none-elf-gcc` (xPack) and
[ch32v003fun](https://github.com/cnlohr/ch32v003fun) cloned as a sibling of
this repository:

```
Projects/
  TinyKey/          <- this repo
  ch32v003fun/      <- git clone https://github.com/cnlohr/ch32v003fun.git
```

```bash
cd firmware/c
make                       # -> tinykey_led_test.bin / .hex
# or, without make:
powershell -File build.ps1
```

If the framework or toolchain live elsewhere, set `CH32FUN` and
`RISCV_TOOLCHAIN` (PowerShell) or pass `CH32FUN=... PREFIX=...` to `make`.

Current build: **3252 B text + 104 B data = 3356 B** of 16 kB flash, 16 B RAM.

## Flash

```bash
make flash
# or
timeout 30 minichlink -w tinykey_led_test.bin flash -b
```

Windows: `powershell -File flash.ps1` (same requirements as the Arduino
firmware: WCH-Link in debug mode, WinUSB bound - see `docs/flashing.md`).

## Configuration

| Macro | Default | Meaning |
|---|---|---|
| `MSG` | `"Nam Khanh"` | scrolling message |
| `POL_REVERSED` | `1` | LEDs soldered backwards; set `0` for a corrected board |
| `DWELL_US` | `250` | per-row dwell; frame is always `7 * DWELL_US` |
| `STEP_MS` | `50` | animation step for everything except the text |
| `SCROLL_MS` | `20` | text advance interval (one column per step) |

The renderer holds the dwell for **every** row, including empty ones. Do not
reintroduce a `continue` for empty rows: brightness is `dwell / frame_time`,
so skipping rows makes the panel pump at the scroll rate.

## Effects

`BTN1` cycles them, `BTN2` sleeps/wakes.

| # | Effect | Notes |
|---|---|---|
| 0 | `FX_SCROLL` | message scrolling right to left |
| 1 | `FX_WAVE` | same text with a sine vertical offset |
| 2 | `FX_SINE` | travelling sine wave |
| 3 | `FX_RIPPLE` | expanding rings from the centre |
| 4 | `FX_FIRE` | per-column flame height, flickering |
| 5 | `FX_RAIN` | falling drops with a short trail |
| 6 | `FX_SPARKLE` | random twinkle |
| 7 | `FX_HEART` | beating heart |
| 8 | `FX_BALL` | bouncing ball |
| 9 | `FX_CHEVRON` | `>` arrows marching right |
| 10 | `FX_SPINNER` | rotating radius |
| 11 | `FX_CHECKER` | animated checkerboard |
| 12 | `FX_BOX` | expanding / contracting square |
| 13 | `FX_BLINK` | whole display blink |

With no current-limiting resistors the display is dim, so effects with many lit
pixels and obvious motion read best (fire, ripple, chevron, checkerboard,
blink, text). Single-dot effects are the weakest - one LED is not much light.

There is deliberately no boot blink: the display goes straight to effect 0, so
nothing looks like "the button blinked an LED".
