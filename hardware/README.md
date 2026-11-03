# Hardware

KiCad 10 project for the TinyKey keychain: CH32V003F4P6, a 7x7 LED matrix and a
TP4056 charger on a two-layer board about 35 x 45 mm. Open
`TinyKey.kicad_pro`; the schematic and PCB sit next to it.

| File | What it is |
|---|---|
| `TinyKey.kicad_pro` | project |
| `TinyKey.kicad_sch` | schematic (USB, MCU, buttons, LED matrix, charger) |
| `TinyKey.kicad_pcb` | current board, neck and 5 mm keyring hole |
| `bom.csv` | bill of materials, grouped by value/footprint |
| `gerbers/` | gerbers, job file and PTH/NPTH drills |
| `TinyKeyV2_gerbers.zip` | the same plot zipped for a fab house |
| `archive/` | earlier outlines and revisions |

## Reading it without KiCad

- `../docs/hardware/TinyKey-schematic.pdf`
- `../docs/hardware/TinyKey-pcb-top.pdf` and `-bottom.pdf`
- `../docs/images/` - schematic PNG and 3D renders
- `../docs/hardware.md` - pin map, option bytes, current limits

## Fabrication notes

- Two layers. The USB-C shell drills are **0.7 mm**, and the board was checked
  with hole clearance at 0.16 mm.
- The neck has a 5 mm hole with 2.5 mm of material between the hole and the
  board edge.
- The LED matrix has **no series resistors**; see `../docs/hardware.md`.
- `BTN1` is on PD7/NRST, so write the option bytes before expecting the button
  to work: `../docs/hardware.md#pd7-is-nrst-by-default`.

DRC is clean apart from harmless warnings: library-footprint mismatches and two
0.7 mm silkscreen texts (the minimum is 0.8 mm). 0 errors, 0 unconnected.

## Renders

The images in `../docs/images/` are generated from the board file, so they stay
in sync:

```bash
kicad-cli pcb render --output docs/images/board-top.png --side top \
  --width 1600 --height 1200 --quality high --background opaque --floor \
  hardware/TinyKey.kicad_pcb
```

The PDFs are exported with `kicad-cli sch export pdf` and
`kicad-cli pcb export pdf` (layers `F.Cu,F.Silkscreen,Edge.Cuts`, and the
mirrored `B.*` set for the bottom).
