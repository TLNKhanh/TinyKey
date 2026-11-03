// TinyKey v2 - 7x7 LED matrix keychain, CH32V003F4P6
//
// Hardware (details in docs/hardware.md):
//   rows     (anodes)    PC0..PC6        active LOW
//   columns  (cathodes)  PC7, PD0, PD2..PD6   active HIGH
//   BTN1  PD7  cycle effect
//   BTN2  PA1  sleep / wake
//
// There is no display driver and no series resistor on the matrix, so the whole
// display is a software row scan: switch on one row, drive that row's columns,
// hold for PWM_US, move to the next row. One frame is 7 rows x 15 PWM steps x
// 16 us, about 1.7 ms, so the matrix refreshes around 590 Hz and nobody sees it
// flicker. Current is limited by the 2 MHz GPIO drive, not by parts.
//
// File layout, top to bottom:
//   config and pin masks
//   buffers (one bitmap, one PWM value per LED)
//   low level: blank the matrix, scan one frame
//   blending: fade the PWM values toward the bitmap
//   font and UTF-8 helpers
//   frame builders: text scroll plus the nine generated effects
//   buttons, sleep, setup and loop

#include <Arduino.h>

// ---------------------------------------------------------------- config ---

#define SPEED_MS      55   // ms per scroll step / effect step
#define BRIGHTNESS    15   // 0..LMAX, scales every PWM value

static const char* MSGS[] = {
    "Hello World",
};
#define MSG_COUNT ((int)(sizeof(MSGS)/sizeof(MSGS[0])))

#define NIB(p)        (0xFu << ((p) * 4))   // 4-bit pin config field for pin p

// Column 0 lives on PC7, columns 1..6 on these PD pins.
static const uint8_t K_DPIN[6] = {0, 2, 3, 4, 5, 6};

#define D_COL_MASK     ((1u<<0)|(1u<<2)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<6))
#define D_COL_MASK_NIB (NIB(0)|NIB(2)|NIB(3)|NIB(4)|NIB(5)|NIB(6))

#define ROW_MASK      0x007Fu   // PC0..PC6, the seven rows
#define COL0_BIT      (1u << 7) // PC7, column 0

#define DEBOUNCE_MS   20
#define PWM_STEPS     15    // brightness levels per LED
#define LMAX          PWM_STEPS
#define PWM_US        16    // hold time per PWM step

#define NON_SCROLL_FX 9     // generated effects after the text
#define TOTAL_STEPS   (MSG_COUNT + NON_SCROLL_FX)

#define BTN1_PIN       PD7
#define BTN2_PIN       PA1
#define BTN1_PIN_NIB   NIB(7)
#define BTN2_PIN_NIB   NIB(1)
#define BTN1_OUTDR_BIT (1u << 7)
#define BTN2_OUTDR_BIT (1u << 1)

#define ABS(x)         ((x) < 0 ? -(x) : (x))

// --------------------------------------------------------------- buffers ---

static uint8_t  fb[7];        // current picture, one bit per LED, row major
static uint8_t  px[7][7];     // faded PWM value per LED (what blendFrame moves)
static uint8_t  disp[7][7];   // value the scan actually shows (px * brightness)
static uint16_t g_msg_cols[MSG_COUNT];  // text length in columns, 6 per char

static uint32_t current_step = 0;   // 0..MSG_COUNT-1 text, then effects
static uint32_t current_p    = 0;   // position inside the current step
static uint32_t lastFrame    = 0;
static uint32_t lastBlend    = 0;

// ------------------------------------------------------------- low level ---

// All rows high (off), all columns low (off). Also used before sleeping.
static void allOff() {
    GPIOC->OUTDR = ROW_MASK;
    GPIOD->OUTDR &= ~D_COL_MASK;
}

// One complete frame: for every row, run 15 PWM steps and light the columns
// whose value is above the current step. The row is switched off again after
// its 15 steps, so only one row conducts at a time.
static void renderFrame() {
    for (uint8_t r = 0; r < 7; r++) {
        uint32_t rowC = ROW_MASK & ~(1u << r);   // pull this row low = on
        for (uint8_t s = 0; s < PWM_STEPS; s++) {
            uint32_t outC = rowC;
            if (disp[r][0] > s) outC |= COL0_BIT;
            uint32_t outD = 0;
            for (uint8_t c = 1; c < 7; c++)
                if (disp[r][c] > s) outD |= (1u << K_DPIN[c - 1]);
            GPIOC->OUTDR = outC;
            GPIOD->OUTDR = (GPIOD->OUTDR & ~D_COL_MASK) | outD;
            delayMicroseconds(PWM_US);
        }
        GPIOC->OUTDR = ROW_MASK;
        GPIOD->OUTDR &= ~D_COL_MASK;
    }
}

// Move every LED a quarter of the way toward its target (on or off) and apply
// the brightness scale. Called every 15 ms, which is what gives the text its
// soft edges instead of hard on/off pixels.
static void blendFrame() {
    for (uint8_t r = 0; r < 7; r++)
        for (uint8_t c = 0; c < 7; c++) {
            uint8_t tgt = (fb[r] & (1u << c)) ? LMAX : 0;
            px[r][c]    = (uint8_t)((px[r][c] + 3u * tgt + 2u) >> 2);
            disp[r][c]  = (uint8_t)(((uint16_t)px[r][c] * BRIGHTNESS + (LMAX / 2)) / LMAX);
        }
}

// ------------------------------------------------------------------ font ---
// 5 columns per glyph, left to right, bit 0 = top row. str_col() adds a sixth
// blank column between characters.

static const char FONT_CHARS[] = " !'-.,:?0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static const uint8_t FONT[] = {
    0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, 0x00,0x07,0x00,0x00,0x00,
    0x08,0x08,0x08,0x08,0x08, 0x00,0x60,0x60,0x00,0x00, 0x00,0x60,0x20,0x00,0x00,
    0x00,0x36,0x36,0x00,0x00, 0x02,0x01,0x51,0x09,0x06, 0x3E,0x41,0x49,0x41,0x3E,
    0x00,0x42,0x7F,0x40,0x00, 0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31,
    0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x30,
    0x01,0x71,0x09,0x05,0x03, 0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E,
    0x7E,0x09,0x09,0x09,0x7E, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
    0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01,
    0x3E,0x41,0x49,0x49,0x7B, 0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00,
    0x20,0x40,0x41,0x7F,0x01, 0x7F,0x08,0x14,0x22,0x41, 0x7F,0x40,0x40,0x40,0x40,
    0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
    0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46,
    0x46,0x49,0x49,0x49,0x31, 0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F,
    0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F, 0x63,0x14,0x08,0x14,0x63,
    0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43, 0x20,0x54,0x54,0x54,0x78,
    0x7F,0x48,0x44,0x44,0x38, 0x38,0x44,0x44,0x44,0x20, 0x38,0x44,0x44,0x48,0x7F,
    0x38,0x54,0x54,0x54,0x18, 0x08,0x7E,0x09,0x01,0x02, 0x08,0x14,0x54,0x54,0x3C,
    0x7F,0x08,0x04,0x04,0x78, 0x00,0x44,0x7D,0x40,0x00, 0x20,0x40,0x44,0x3D,0x00,
    0x7F,0x10,0x28,0x44,0x00, 0x00,0x00,0x7F,0x00,0x00, 0x7C,0x04,0x18,0x04,0x78,
    0x7C,0x08,0x04,0x04,0x78, 0x38,0x44,0x44,0x44,0x38, 0x7C,0x14,0x14,0x14,0x08,
    0x08,0x14,0x14,0x18,0x7C, 0x7C,0x08,0x04,0x04,0x08, 0x48,0x54,0x54,0x54,0x20,
    0x04,0x3F,0x44,0x40,0x20, 0x3C,0x40,0x40,0x20,0x7C, 0x1C,0x20,0x40,0x20,0x1C,
    0x3C,0x40,0x30,0x40,0x3C, 0x44,0x28,0x10,0x28,0x44, 0x0C,0x50,0x50,0x50,0x3C,
    0x44,0x64,0x54,0x4C,0x44
};

// "a" with a grave and an acute accent, so MSGS can hold real UTF-8 Vietnamese
// text. Two bytes in, one glyph out.
static const uint8_t GLYPH_A_GRAVE[5] = {0x20,0x55,0x56,0x54,0x78};
static const uint8_t GLYPH_A_ACUTE[5] = {0x20,0x56,0x55,0x54,0x78};

// Index into FONT, or 7 ('?') for anything the font does not have.
static int font_index(uint8_t ch) {
    for (int i = 0; FONT_CHARS[i]; i++)
        if ((uint8_t)FONT_CHARS[i] == ch) return i;
    return 7;
}

// Length in characters. This font only sees 0xC3 as a lead byte, which covers
// the Latin-1 accented letters used here.
static int utf8_len(const char* s) {
    int n = 0, i = 0;
    while (s[i]) { i += ((uint8_t)s[i] == 0xC3) ? 2 : 1; n++; }
    return n;
}

// Byte offset of character ch_idx in a UTF-8 string.
static int utf8_byte_offset(const char* s, int ch_idx) {
    int i = 0;
    while (s[i] && ch_idx > 0) {
        i += ((uint8_t)s[i] == 0xC3) ? 2 : 1;
        ch_idx--;
    }
    return i;
}

// One column of one character, 0..6 (6 is the gap between glyphs).
static uint8_t str_col(const char *s, int len6, int i) {
    if (i < 0 || i >= len6) return 0;
    int ch = i / 6, k = i % 6;
    if (k == 5) return 0;
    int off = utf8_byte_offset(s, ch);
    if (s[off] == 0) return 0;
    if ((uint8_t)s[off] == 0xC3) {
        uint8_t b2 = (uint8_t)s[off + 1];
        if (b2 == 0)    return 0;
        if (b2 == 0xA0) return GLYPH_A_GRAVE[k];
        if (b2 == 0xA1) return GLYPH_A_ACUTE[k];
        return 0;
    }
    return FONT[font_index((uint8_t)s[off]) * 5 + k];
}

// Triangle wave 0..LMAX with a 16 step period, used by every generated effect.
static uint8_t wave_val(int x) {
    int w = x % 16;
    if (w < 0) w += 16;
    if (w > 8) w = 16 - w;
    return (w * LMAX) / 8;
}

// ---------------------------------------------------------- frame builder ---

// Fill fb (or disp) for one step. Text scrolls right to left and goes through
// blendFrame for smooth edges. The generated effects write disp directly, they
// are already soft because wave_val returns a PWM level.
static void build_frame(uint8_t step, uint32_t p) {
    uint8_t r, c;

    if (step < MSG_COUNT) {
        for (r = 0; r < 7; r++) fb[r] = 0;
        int len6 = (int)g_msg_cols[step];
        // +6 so the text starts off screen on the right, +7 so it leaves fully.
        uint32_t pos = (p + 6) % ((uint32_t)len6 + 7u);
        for (c = 0; c < 7; c++) {
            uint8_t col = str_col(MSGS[step], len6, (int)pos + (int)c - 6);
            for (r = 0; r < 7; r++) if (col & (1u << r)) fb[r] |= (uint8_t)(1u << c);
        }
        return;
    }

    uint8_t fx = (uint8_t)(step - MSG_COUNT);

    // fx 2: two chevrons scrolling down, drawn from a bitmap so it needs the
    // normal blend path.
    if (fx == 2) {
        static const uint8_t ARR[7] = {0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60};
        int x1 = 7 - (int)(p % 14);
        int x2 = 7 - (int)((p + 7) % 14);
        for (r = 0; r < 7; r++) {
            uint8_t v = ARR[r];
            uint8_t out = 0;
            if (x1 >= 0 && x1 < 7) out |= (v << x1) & 0x7F;
            else if (x1 < 0 && x1 > -7) out |= (v >> -x1) & 0x7F;
            if (x2 >= 0 && x2 < 7) out |= (v << x2) & 0x7F;
            else if (x2 < 0 && x2 > -7) out |= (v >> -x2) & 0x7F;
            fb[r] = out;
        }
        return;
    }

    // The rest are distance based: pick a distance from the centre, turn it into
    // a wave value, done. dr/dc are -3..3 from the middle LED.
    for (r = 0; r < 7; r++) {
        for (c = 0; c < 7; c++) {
            uint8_t val = 0;
            int dr = r - 3;
            int dc = c - 3;

            switch (fx) {
                case 0: {   // square rings
                    int d = ABS(dr) > ABS(dc) ? ABS(dr) : ABS(dc);
                    val = wave_val(d * 4 - p);
                    break;
                }
                case 1: {   // diagonal stripes
                    val = wave_val(r * 2 + c * 2 + p);
                    break;
                }
                case 3: {   // diamonds
                    int d = ABS(dr) + ABS(dc);
                    val = wave_val(d * 3 - p);
                    break;
                }
                case 4: {   // circles
                    int d2 = dr*dr + dc*dc;
                    val = wave_val(d2 * 2 - p);
                    break;
                }
                case 5: {   // diagonal lines
                    int d = ABS(ABS(dr) - ABS(dc));
                    val = wave_val(d * 4 - p);
                    break;
                }
                case 6: {   // cross
                    int d = ABS(dr) < ABS(dc) ? ABS(dr) : ABS(dc);
                    val = wave_val(d * 4 - p);
                    break;
                }
                case 7: {   // two axes interfering
                    uint8_t v1 = wave_val(r * 4 + p);
                    uint8_t v2 = wave_val(c * 4 - p);
                    val = (v1 + v2) / 2;
                    break;
                }
                case 8: {   // frame pulse
                    int d = ABS(dr) > ABS(dc) ? ABS(dr) : ABS(dc);
                    int fd = ABS(d - 3);
                    val = wave_val(fd * 4 - p);
                    break;
                }
            }
            disp[r][c] = (uint8_t)(((uint16_t)val * BRIGHTNESS + (LMAX / 2)) / LMAX);
        }
    }
}

// --------------------------------------------------------------- buttons ---
// Both buttons are pulled up, so a pressed button reads 0 on the input register.

static inline uint8_t readB1() { return (GPIOD->INDR & BTN1_OUTDR_BIT) ? 0 : 1; }
static inline uint8_t readB2() { return (GPIOA->INDR & BTN2_OUTDR_BIT) ? 0 : 1; }

static void wake_isr() { }   // only here so EXTI has somewhere to return to

// Blank the display, wait for BTN2 to be released (so the press that put us to
// sleep does not immediately wake us), then stop SysTick and halt the core with
// WFI. A falling edge on BTN2 wakes it, SysTick restarts, and the frame timers
// are reset so nothing thinks a whole sleep worth of time has passed.
static void enterSleepMode() {
    allOff();
    while (readB2()) {
        delay(10);
    }
    delay(50);
    SysTick->CTLR = 0;
    __WFI();
    SysTick->CTLR = 0xF;
    lastFrame = millis();
    lastBlend = millis();
}

// ----------------------------------------------------------------- setup ---

void setup() {
    delay(100);   // let the rails settle before driving the matrix

    // Clocks for GPIOA, GPIOC, GPIOD and AFIO.
    RCC->APB2PCENR |= (1u << 0) | (1u << 2) | (1u << 4) | (1u << 5);

    // millis() needs SysTick. The Arduino core normally starts it; this is the
    // fallback for when it has not, and it assumes the 24 MHz HSI clock.
    if ((SysTick->CTLR & 0x1u) == 0) {
        SystemCoreClock = 24000000UL;
        SysTick->CTLR = 0;
        SysTick->CNT  = 0;
        SysTick->CMP  = SystemCoreClock / 1000UL - 1UL;
        SysTick->SR   = 0;
        SysTick->CTLR = 0xF;
    }

    // PD7 is NRST from the factory. This only works once the option bytes have
    // been written (USER = 0x1F); see docs/hardware.md.
    AFIO->PCFR1 &= ~(1u << 15);

    // Rows and columns: push-pull output, 2 MHz. Do not raise the speed, the
    // matrix has no series resistors and the pins are the current limit.
    GPIOC->CFGLR = 0x22222222u;
    GPIOD->CFGLR = (GPIOD->CFGLR & ~D_COL_MASK_NIB) | (0x22222222u & D_COL_MASK_NIB);

    // BTN1 and BTN2: input with pull-up.
    GPIOD->CFGLR = (GPIOD->CFGLR & ~BTN1_PIN_NIB) | (0x8u << (7 * 4));
    GPIOD->OUTDR |= BTN1_OUTDR_BIT;
    GPIOA->CFGLR = (GPIOA->CFGLR & ~BTN2_PIN_NIB) | (0x8u << (1 * 4));
    GPIOA->OUTDR |= BTN2_OUTDR_BIT;

    // Text length in columns, computed once.
    for (int i = 0; i < MSG_COUNT; i++)
        g_msg_cols[i] = (uint16_t)(utf8_len(MSGS[i]) * 6);

    // BTN2 also wakes the core from WFI, so it needs an EXTI line.
    attachInterrupt(BTN2_PIN, GPIO_Mode_IPU, wake_isr, EXTI_Mode_Interrupt, EXTI_Trigger_Falling);

    lastFrame = millis();
    lastBlend = millis();
    allOff();
}

// ------------------------------------------------------------------ loop ---

void loop() {
    static uint8_t  b1prev    = 0, b2prev = 0;
    static uint32_t b1lock    = 0, b2lock = 0;
    uint32_t ms = millis();
    uint8_t  b1 = readB1();
    uint8_t  b2 = readB2();

    // BTN2: sleep. The call blocks until the button is released and returns
    // after the wake-up press, so nothing else has to know about it.
    if (b2 && !b2prev && (uint32_t)(ms - b2lock) >= DEBOUNCE_MS) {
        b2lock = ms;
        enterSleepMode();
    }
    b2prev = b2;

    // BTN1: next step, and clear the fade buffers so text fades in from black.
    if (b1 && !b1prev && (uint32_t)(ms - b1lock) >= DEBOUNCE_MS) {
        b1lock = ms;
        current_step = (current_step + 1) % TOTAL_STEPS;
        current_p = 0;
        lastFrame = ms - SPEED_MS;
        lastBlend = ms - 15u;
        for (int r = 0; r < 7; r++) {
            fb[r] = 0;
            for (int c = 0; c < 7; c++) px[r][c] = 0;
        }
    }
    b1prev = b1;

    // Advance the animation on a fixed time base, not per frame.
    if ((uint32_t)(ms - lastFrame) >= SPEED_MS) {
        lastFrame = ms;
        build_frame(current_step, current_p);
        current_p++;
    }

    // Text and the chevrons are bitmap based and get the fade; the distance
    // effects already produce a PWM value per LED.
    if (current_step < MSG_COUNT || (current_step - MSG_COUNT) == 2) {
        if ((uint32_t)(ms - lastBlend) >= 15u) {
            lastBlend = ms;
            blendFrame();
        }
    }

    renderFrame();
}
