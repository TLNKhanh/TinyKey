# Photo notes

The README shows real photos of both revisions, shot on a Galaxy A25. The
V2 board (manufactured Nov 2026) is the current one: front with the matrix
lit, back with the cell. Everything else in `docs/images/v2/` is generated
from the V2 KiCad files. `docs/images/` is split per revision:

- `docs/images/v1/` - the V1 prototype (40 x 52 mm)
- `docs/images/v2/` - the V2 board (35 x 45.4 mm, current)

## What is in the repo

| File | Status | What |
|---|---|---|
| `v1/photo-front.jpg` | done, V1 | assembled prototype, LEDs running |
| `v1/photo-back.jpg` | done, V1 | back side with the cell, TP4056, USB-C, buttons |
| `v2/photo-front.jpg` | done, V2 | manufactured board, matrix lit |
| `v2/photo-back.jpg` | done, V2 | back side with the cell, TP4056, USB-C, buttons |
| `v2/board-top.png`, `v2/board-bottom.png` | render, V2 | KiCad 3D render of the bare board |
| `v2/board-hero.jpg` | render, V2 | 3/4 render, kept as the fallback hero |
| `v2/schematic.png` | export, V2 | schematic preview used in the README |

## Still missing

| File | When | What |
|---|---|---|
| `demo.gif` | recording | 5-8 s loop of the effects, LEDs lit |
| `v2/photo-bare-front.jpg` | next batch | LED side before soldering |
| `v2/photo-bare-back.jpg` | next batch | charger/MCU side before soldering |

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
