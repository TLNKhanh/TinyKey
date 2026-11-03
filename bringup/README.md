# Bring-up sketches

Six minimal Arduino sketches, in the order they were used on the first
prototype. They are not useful as a keychain firmware; they are useful when a
new board misbehaves.

| Sketch | What it checks |
|---|---|
| `led_test1` | one LED (D1) lit statically - proved the LEDs are mounted backwards |
| `led_test2` | D1 blinking with `micros()` |
| `led_test3` | switching a line between output and floating input (high-Z) |
| `led_test4` | first row scan, all 49 LEDs on |
| `led_test5` | blink driven by `millis()` (used while checking the core's SysTick) |
| `led_test6` | row scan with a moving text column, `fb`/`buf` double buffer |

All of them assume the wiring in `docs/hardware.md` and the reversed LED
polarity (anode LOW, cathode HIGH).
