# Photo notes

The README uses two real photos (`photo-front.jpg`, `photo-back.jpg`, shot on a
Galaxy A25 in Nov 2026). Both show the **V1** prototype, the board that was
actually manufactured. Everything else in `docs/images/` is generated from the
V2 KiCad files.

## What is in the repo

| File | Status | What |
|---|---|---|
| `photo-front.jpg` | done, V1 | assembled board, LEDs running, keyring on |
| `photo-back.jpg` | done, V1 | back side with the cell, TP4056, USB-C, buttons |
| `board-top.png`, `board-bottom.png` | render, V2 | KiCad 3D render of the bare board |
| `board-hero.jpg` | render, V2 | 3/4 render, kept as the fallback hero |
| `schematic.png` | export, V2 | schematic preview used in the README |

## Still missing

| File | When | What |
|---|---|---|
| `demo.gif` | assembled | 5-8 s loop of the effects, LEDs lit |
| `photo-bare-front.jpg` | next fab run | LED side before soldering |
| `photo-bare-back.jpg` | next fab run | charger/MCU side before soldering |

## Shooting them

- Hero: dark, non-reflective surface, one soft light from above and slightly
  behind the camera. Phone in portrait, crop to about 4:5.
- Underexpose slightly when the matrix is lit, otherwise the LEDs bloom into
  white blobs and you lose the row structure.
- The back side is the hard one: the cell is matt silver and the silk is white,
  so use a diffused light and keep it off-axis or the silkscreen blows out.
- Demo GIF: record 5-8 s at 30 fps, crop to the board, resize to 800-1000 px
  wide, keep it under 10 MB so GitHub autoplays it.
- Strip EXIF before committing (GPS, phone model): `exiftool -all= photo.jpg`.
  The two photos here were re-encoded through Pillow, which drops EXIF too.
- Keep originals out of the repo; commit the resized versions only.

When swapping a photo in, update the table in `README.md` and this file.
