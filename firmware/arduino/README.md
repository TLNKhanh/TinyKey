# Arduino firmware

## `tinykey_v2/` - current firmware

The main sketch. Board: **CH32V00x EVT**, part **CH32V003F4**:

```bash
arduino-cli compile --fqbn WCH:ch32v:CH32V00x_EVT:pnum=CH32V003F4 \
  --output-dir out tinykey_v2
timeout 30 minichlink -w out/tinykey_v2.ino.bin flash -b
```

9172 B flash / 572 B RAM (55 % / 27 % of the CH32V003). The `--output-dir`
step matters: the WCH core's own uploader uses OpenOCD, which is flaky here -
compile with `arduino-cli` and flash the `.bin` with minichlink.

The sketch is commented section by section, so this file only covers the parts
that are easy to miss.

### How it works

- **Row scan with 15-step PWM.** Each row is driven for 15 slots of 16 us
  (240 us per row, 1.68 ms per 7-row frame, ~595 Hz). `fb[r]` is the 1-bit
  target image, `px[r][c]` the faded value, `disp[r][c]` what the scan shows.
- **Temporal blend.** Every 15 ms each pixel moves a quarter of the way to its
  target (`(px + 3*tgt + 2) >> 2`). That is where the soft text edges and the
  sub-pixel scrolling come from.
- **Brightness.** `BRIGHTNESS 15` equals `LMAX 15`, so the display runs at full
  duty. Lower it for a dimmer, longer-running keychain; the scan timing does
  not change.
- **Timing.** `SPEED_MS 55` is one animation step. `setup()` re-arms SysTick
  only if the core has not started it, and assumes the 24 MHz HSI clock.
- **Buttons.** `BTN1` (PD7) = next step, `BTN2` (PA1) = sleep/wake, 20 ms
  debounce, active low with internal pull-ups. `BTN1` only works because the
  option bytes disable NRST (see `docs/hardware.md`).
- **Sleep.** `enterSleepMode()` blanks the matrix, waits for `BTN2` to be
  released, stops SysTick and executes `WFI`. A falling edge on `BTN2` wakes
  the core through EXTI; SysTick restarts and the frame timers reset.

### Effects

Step 0 is the text scroll, steps 1 to 9 are generated patterns:

| Step | What it does |
|---|---|
| 0 | scrolls `MSGS[]` right to left |
| 1 | square rings |
| 2 | diagonal stripes |
| 3 | two chevrons marching down |
| 4 | diamonds |
| 5 | circles |
| 6 | diagonal lines |
| 7 | cross |
| 8 | two axes interfering |
| 9 | frame pulse |

The text and the chevrons are bitmap based and go through the fade; the
distance patterns compute a PWM value per LED directly.

### Changing the text

Edit `MSGS[]` near the top of `tinykey_v2.ino`:

```cpp
static const char* MSGS[] = {
    "Hello World",
};
```

Each character is 6 columns wide (5 pixels + 1 gap). The font covers space,
digits, `A-Z`, `a-z` and a few punctuation marks. Text is decoded as UTF-8;
`á` and `à` have their own glyphs, anything else outside the font shows as `?`.
Add more strings and `MSG_COUNT` picks them up, so BTN1 cycles through them
before the patterns.

## `legacy/` - earlier sketches

Kept because each one solved a specific problem; none are maintained.

| Sketch | Notes |
|---|---|
| `tinykey_arduino` | first full Arduino port: 14 effects + a button test effect, 9416 B |
| `led_keychain` | 7 effects, auto-dim after 30 s, charge-status handling |
| `led_keychain2` | 14 effects, `Nam Khanh` text |
| `led_keychain3` | 13 effects, long-press BTN1 to sleep (used while BTN2 was suspect) |
| `bounce_pwm` | first grayscale version: 15-step PWM over 1-bit effects |
| `bounce_pwm2` | decoupled the text scroll from `STEP_MS` with sub-pixel steps |

All of them use the same wiring and the same reversed-polarity row scan.
