# Photo shot list

The README currently uses 3D renders generated from the board file. Real photos
will look better, but only add them when you have them - do not use stock
images of a different board.

Suggested set, in order of importance:

| File | When | What |
|---|---|---|
| `photo-hero.jpg` | assembled + firmware running | the keychain at a 3/4 angle with the LEDs lit, dark background, text readable |
| `photo-back.jpg` | assembled | back side: USB-C, buttons, MCU, keyring in place |
| `photo-bare-front.jpg` | bare PCB from the fab | LED side, before soldering |
| `photo-bare-back.jpg` | bare PCB from the fab | charger/MCU side, before soldering |
| `demo.gif` | assembled | 5-8 s loop of the effects, LEDs lit |

Practical notes:

- Shoot the hero on a dark, non-reflective surface, one soft light from above
  and slightly behind the camera; a phone in portrait is fine, crop to 3:2
  landscape (~2400 px wide).
- Use a tripod or rest the phone - the matrix is bright but the exposure is
  long in a dark room. Underexpose slightly so the lit LEDs do not bloom into
  white blobs.
- For the demo GIF, record 5-8 s at 30 fps, crop to the board, resize to
  800-1000 px wide and keep it under 10 MB so GitHub autoplays it.
- Strip EXIF (especially GPS) before committing:
  `exiftool -all= photo-hero.jpg`.
- Keep the original photos out of the repo; only commit the resized versions.

Add the hero at the top of the README (replace the `board-hero.png` line) and
put the rest in the Pictures section. Update `photo-*` names here if you use
different ones.
