# Changelog

Dates are when the work happened on the bench, not release dates.

## 2026-11-03 - repository cleanup

- Split the project into `firmware/`, `hardware/`, `docs/`, `tools/` and
  `prebuilt/`.
- Fixed `render_grid()` in the C firmware: the per-row dwell is now held for
  empty rows too, so the frame time (and therefore brightness) no longer
  follows the picture while the text scrolls.
- Added readme/docs, MIT license, flashing helpers and reproducible prebuilt
  binaries.
- Added 3D renders, schematic and PCB PDFs, a BOM export, and filled in the
  schematic title block (TinyKey, rev v2, author).
- Scroll text changed to `Nam Khanh` and the prebuilt Arduino binary rebuilt
  (8932 B flash, 9100 B bin).
- Added real photos of the assembled keychain (front with the LEDs running,
  back with the cell taped over the charger), published the repo as
  [TLNKhanh/TinyKey](https://github.com/TLNKhanh/TinyKey), then rewrote the
  README around the ten cent MCU and the V1 (built) vs V2 (not built) split.

## 2026-11-01 - hardware v2

- Added the keyring neck and 5 mm hole, reworked the ground zones; board is
  ~35 x 45.4 mm with 0 errors / 0 unconnected in DRC.
- Exported gerbers and a fab zip (`hardware/gerbers`, `hardware/TinyKeyV2_gerbers.zip`).

## 2026-10-22 - `tinykey_v2`

- Rewrite of the Arduino firmware: 15-step PWM, sub-pixel text scroll, constant
  dwell row scan, 8 effects (spinner/checker/heat removed), positive-only face
  effect, `Tà Mi` text with a UTF-8 `à` glyph.
- 8928 B flash / 544 B RAM.

## 2026-10-19 - Arduino port

- First working Arduino (WCH core) firmware, row-scanned, `Nam Khanh` text and
  14 effects.
- Option bytes rewritten (`USER 0x17 -> 0x1F`) so `BTN1` on PD7 stops resetting
  the MCU.
- Flash via `minichlink` from Linux; Windows WinUSB binding documented.

## 2026-10-18 - C firmware

- ch32v003fun firmware with 14 effects, 5x7 font and button handling,
  3308 B of 16 kB.

## 2026-09 - bring-up

- Board assembled. All 49 LEDs turned out to be soldered backwards; the fix is
  drive polarity, not rework.
- Discovered that a one-LED-at-a-time scan (1/49 duty) is dim and flickers;
  switched to row scan with a constant 7-step frame.

## Earlier

- `TinyKey-keychain` (archived in `hardware/archive/`) was the first outline,
  before the neck and mounting hole.
