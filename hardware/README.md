# Hardware

KiCad 10 project for the TinyKey keychain (CH32V003F4P6 + 7x7 LED matrix +
TP4056 charger). Open `TinyKey.kicad_pro`; the PCB and schematic sit next to it.

| File | What it is |
|---|---|
| `TinyKey.kicad_pro` | project |
| `TinyKey.kicad_sch` | schematic (one A4 sheet: USB, MCU, buttons, LED matrix, charger) |
| `TinyKey.kicad_pcb` | current board, ~35 x 45.4 mm, two layers |
| `gerbers/` | plotted gerbers, job file and PTH/NPTH drills |
| `TinyKeyV2_gerbers.zip` | the same plot zipped for a fab house |
| `archive/` | earlier outlines/revisions (`TinyKey-keychain`, `TinyKeyV1`, `TinyKey2`) |

PDF and PNG exports for reading without KiCad live in `../docs/hardware/` and
`../docs/images/`.

## Fabrication notes

- Two-layer board, USB-C shell drills are **0.7 mm** (hole clearance checked at
  0.16 mm).
- 5 mm keyring hole in the neck, with 2.5 mm of material between the hole and
  the board edge at the top.
- No series resistors on the LED matrix - see `../docs/hardware.md`,
  "No series resistors".
- `BTN1` is on PD7/NRST, so the option bytes must be written before the button
  behaves: `../docs/hardware.md#pd7-is-nrst-by-default`.

DRC: 0 errors, 0 unconnected. Remaining warnings are library-footprint
mismatches and two 0.7 mm silkscreen texts (minimum 0.8 mm); neither blocks
fabrication.

## Renders

The pictures in `../docs/images/` are generated from the board file, so they
never go stale:

```bash
kicad-cli pcb render --output docs/images/board-top.png --side top \
  --width 1600 --height 1200 --quality high --background opaque --floor \
  hardware/TinyKey.kicad_pcb
```
