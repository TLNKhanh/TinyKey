# Arduino firmware

## `tinykey_v2/` - current firmware

The main sketch. Board: **CH32V00x EVT**, part **CH32V003F4**:

```bash
arduino-cli compile --fqbn WCH:ch32v:CH32V00x_EVT:pnum=CH32V003F4 \
  --output-dir out tinykey_v2
timeout 30 minichlink -w out/tinykey_v2.ino.bin flash -b
```

8932 B flash / 544 B RAM (54 % / 26 % of the CH32V003). The `--output-dir`
step matters: the WCH core's own uploader uses OpenOCD, which is flaky here -
compile with `arduino-cli` and flash the `.bin` with minichlink.

### How it works

- **Row scan with 15-step PWM.** Each row is driven for 15 slots of 16 us
  (240 us per row, 1.68 ms per 7-row frame, ~595 Hz). `px[r][c]` holds the
  brightness of each LED; `fb[r]` is the 1-bit target image.
- **Temporal blend.** Every 15 ms each pixel moves one step toward its target
  (`(px + tgt + 1) >> 1`), which gives fading and sub-pixel text motion without
  a bigger framebuffer.
- **Brightness.** `BRIGHT 7` of `PWM_STEPS 15` is about 47 % duty per lit LED.
  Lower `BRIGHT` for a dimmer display; the scan timing does not change.
- **Timing.** `STEP_MS 85` is the animation step; the text advances one pixel
  column per step. `setup()` sets `SystemCoreClock = 24 MHz` and re-arms
  SysTick because the WCH core never calls `SystemInit()`; without that
  `millis()` runs 3x too fast.
- **Buttons.** `BTN1` (PD7) = next effect, `BTN2` (PA1) = sleep/wake. Both are
  active low with internal pull-ups and 20 ms debounce. `BTN1` only works
  because the option bytes disable NRST (see `docs/hardware.md`).

### Effects

| # | Name | What it does |
|---|---|---|
| 0 | `FX_TEXT` | scrolls `TEXTS[]` right to left, cycling messages |
| 1 | `FX_WAVE` | a two-pixel sine band travelling across the panel |
| 2 | `FX_RIPPLE` | expanding square rings from the centre |
| 3 | `FX_RAIN` | three-pixel drops falling in staggered columns |
| 4 | `FX_HEART` | 7x7 heart, beating between two sizes |
| 5 | `FX_BALL` | fixed-point two-axis bounce with a two-pixel trail |
| 6 | `FX_CHEVRON` | `>` arrows marching right |
| 7 | `FX_FACE` | seven expressions (happy, surprised, sleepy, wink, grin, love, uwu) with random holds and blinks |

### Changing the text

Edit `TEXTS[]` near the top of `tinykey_v2.ino`:

```cpp
static const char *const TEXTS[] = {
    "Nam Khanh",
};
```

Each character is 6 columns wide (5 pixels + 1 space). The font covers space,
digits, `A-Z`, `a-z` and a few punctuation marks. Text is decoded as UTF-8;
`à` has a dedicated glyph (`FONT_AGRAVE`), other accented characters fall back
to `?`. Add more strings to cycle through them.

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
