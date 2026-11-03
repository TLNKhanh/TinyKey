/*
 * 7x7 multiplexed LED keychain - CH32V003 (Arduino core)
 *   Text: "Nam Khanh"  + 13 effects (wave removed)
 *   BTN1 (PD7) = next effect (short) / sleep (long >=500ms)
 *   BTN2 (PA1) = not used (broken at hardware level)
 *
 * Wiring:
 *   Anodes   A1..A7 = PC0..PC6
 *   Cathodes K1..K7 = PC7, PD0, PD2, PD3, PD4, PD5, PD6
 *   BTN1 = PD7 (NRST disabled via option byte USER=0x1F)
 *   PD1  = SWIO (never touched)
 *
 * POLARITY: LEDs soldered backwards -> lit = anode LOW + cathode HIGH.
 * CURRENT: no series resistors -> 2 MHz GPIO drive only.
 */

#include <Arduino.h>

#define NIB(p)    (0xFu << ((p) * 4))
#define CFG_FLT   0x4u
#define CFG_OUT2  0x2u
#define CFG_PU    0x8u

static const uint8_t K_DPIN[6] = {0, 2, 3, 4, 5, 6};
#define D_CATH_MASK (NIB(0) | NIB(2) | NIB(3) | NIB(4) | NIB(5) | NIB(6))
#define D_CATH_FLT  (0x44444444u & D_CATH_MASK)
#define D_OUT_MASK  ((1u<<0) | (1u<<2) | (1u<<3) | (1u<<4) | (1u<<5) | (1u<<6))

#define ROW_US 250UL
#define STEP_MS 65
#define SCROLL_MS 80

static uint8_t fb[7];

static void blankAll() {
  GPIOC->CFGLR = 0x44444444u;
  GPIOD->CFGLR = (GPIOD->CFGLR & ~D_CATH_MASK) | D_CATH_FLT;
}

static void driveRow(uint8_t r) {
  uint8_t cols = fb[r];
  uint32_t cfgC = 0x44444444u, outC = 0;
  uint32_t cfgD = D_CATH_FLT,  outD = 0;
  cfgC = (cfgC & ~NIB(r)) | (CFG_OUT2 << (r * 4));
  for (uint8_t c = 0; c < 7; c++) {
    if (!(cols & (1u << c))) continue;
    if (c == 0) { cfgC = (cfgC & ~NIB(7)) | (CFG_OUT2 << 28); outC |= (1u << 7); }
    else { uint8_t p = K_DPIN[c - 1]; cfgD = (cfgD & ~NIB(p)) | (CFG_OUT2 << (p * 4)); outD |= (1u << p); }
  }
  blankAll();
  if (!cols) return;
  GPIOC->OUTDR = outC;
  GPIOD->OUTDR = (GPIOD->OUTDR & ~D_OUT_MASK) | outD;
  GPIOC->CFGLR = cfgC;
  GPIOD->CFGLR = (GPIOD->CFGLR & ~D_CATH_MASK) | cfgD;
}

// ---------- font ----------
static const char FONT_CHARS[] = " !'-.,:?0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static const uint8_t FONT[] = {
    0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, 0x00,0x07,0x00,0x00,0x00,
    0x08,0x08,0x08,0x08,0x08, 0x00,0x60,0x60,0x00,0x00, 0x00,0x40,0x20,0x00,0x00,
    0x00,0x36,0x36,0x00,0x00, 0x02,0x01,0x51,0x09,0x06,
    0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00, 0x42,0x61,0x51,0x49,0x46,
    0x21,0x41,0x45,0x4B,0x31, 0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39,
    0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03, 0x36,0x49,0x49,0x49,0x36,
    0x06,0x49,0x49,0x29,0x1E,
    0x7E,0x09,0x09,0x09,0x7E, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
    0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01,
    0x3E,0x41,0x41,0x51,0x73, 0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00,
    0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41, 0x7F,0x40,0x40,0x40,0x40,
    0x7F,0x02,0x1C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
    0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46,
    0x46,0x49,0x49,0x49,0x31, 0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F,
    0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F, 0x63,0x14,0x08,0x14,0x63,
    0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43,
    0x20,0x54,0x54,0x54,0x78, 0x7F,0x48,0x44,0x44,0x38, 0x38,0x44,0x44,0x44,0x20,
    0x38,0x44,0x44,0x48,0x7F, 0x38,0x54,0x54,0x54,0x18, 0x08,0x7E,0x09,0x01,0x02,
    0x08,0x14,0x54,0x54,0x3C, 0x7F,0x08,0x04,0x04,0x78, 0x00,0x44,0x7D,0x40,0x00,
    0x20,0x40,0x44,0x3D,0x00, 0x7F,0x10,0x28,0x44,0x00, 0x00,0x41,0x7F,0x40,0x00,
    0x7C,0x04,0x18,0x04,0x78, 0x7C,0x08,0x04,0x04,0x78, 0x38,0x44,0x44,0x44,0x38,
    0x7C,0x14,0x14,0x14,0x08, 0x08,0x14,0x14,0x18,0x7C, 0x7C,0x08,0x04,0x04,0x08,
    0x48,0x54,0x54,0x54,0x20, 0x04,0x3F,0x44,0x40,0x20, 0x3C,0x40,0x40,0x20,0x7C,
    0x1C,0x20,0x40,0x20,0x1C, 0x3C,0x40,0x30,0x40,0x3C, 0x44,0x28,0x10,0x28,0x44,
    0x0C,0x50,0x50,0x50,0x3C, 0x44,0x64,0x54,0x4C,0x44
};

static int font_index(char ch) {
    for (int i = 0; FONT_CHARS[i]; i++) if (FONT_CHARS[i] == ch) return i;
    return 7;
}

static uint32_t hash32(uint32_t x) { x ^= x >> 13; x *= 1274126177u; x ^= x >> 16; return x; }

#define MSG "Nam Khanh"
#define MSG_N ((int)(sizeof(MSG)-1))
#define MSG_COLS (MSG_N * 6)

static uint8_t str_col(const char *s, int i) {
    if (i < 0 || i >= MSG_COLS) return 0;
    int ch = i / 6, k = i % 6;
    if (k == 5) return 0;
    return FONT[font_index(s[ch]) * 5 + k];
}

// ---------- effects ----------
#define FX_COUNT 13
static const int8_t WAVE[16] = {0,1,2,2,3,2,2,1,0,-1,-2,-2,-3,-2,-2,-1};
static const uint8_t CHEV[5] = {0x41,0x22,0x14,0x08,0x08};

static uint32_t g_sp = 0;

static void build_frame(uint8_t fx, uint32_t p) {
    uint8_t r, c;
    for (r = 0; r < 7; r++) fb[r] = 0;
    switch (fx) {
    case 0: { // scroll
        int base = (int)(g_sp % (uint32_t)(MSG_COLS + 7));
        for (c = 0; c < 7; c++) {
            uint8_t col = str_col(MSG, base + (int)c - 6);
            for (r = 0; r < 7; r++) if (col & (1u << r)) fb[r] |= (uint8_t)(1u << c);
        }
        break;
    }
    case 1: { // sine (thickened 2 rows for smoothness)
        for (c = 0; c < 7; c++) {
            int row = 3 + WAVE[(c + (int)p) & 15];
            if (row >= 0 && row < 7) fb[row] |= (uint8_t)(1u << c);
            if (row + 1 >= 0 && row + 1 < 7) fb[row + 1] |= (uint8_t)(1u << c);
        }
        break;
    }
    case 2: { // ripple
        int phase = (int)(p % 5);
        for (r = 0; r < 7; r++)
            for (c = 0; c < 7; c++) {
                int dr = (r > 3) ? (r - 3) : (3 - r);
                int dc = (c > 3) ? (c - 3) : (3 - c);
                int d = (dr > dc) ? dr : dc;
                if (d == phase || d == (phase - 1)) fb[r] |= (uint8_t)(1u << c);
            }
        break;
    }
    case 3: { // fire
        static uint8_t heat[7] = {1,3,5,7,5,3,1};
        static uint32_t lastp = 0xFFFFFFFFu;
        if (p != lastp) {
            lastp = p;
            for (c = 0; c < 7; c++) {
                int8_t d = (int8_t)(hash32(p * 2654435761u + c * 40503u) % 3u) - 1;
                int16_t v = (int16_t)heat[c] + d;
                if (v < 1) v = 1; if (v > 7) v = 7;
                heat[c] = (uint8_t)v;
            }
        }
        for (c = 0; c < 7; c++)
            for (r = 0; r < 7; r++)
                if ((7 - r) <= heat[c]) fb[r] |= (uint8_t)(1u << c);
        break;
    }
    case 4: { // rain
        for (c = 0; c < 7; c++) {
            int drop = (int)((p / 2 + (uint32_t)(c * 5)) % 12) - 4;
            for (int k = 0; k < 3; k++) {
                int rr = drop - k;
                if (rr >= 0 && rr < 7) fb[rr] |= (uint8_t)(1u << c);
            }
        }
        break;
    }
    case 5: { // sparkle
        for (r = 0; r < 7; r++)
            for (c = 0; c < 7; c++)
                if (hash32(p * 2654435761u + (uint32_t)(r * 7 + c) * 2246822519u) & 1u)
                    fb[r] |= (uint8_t)(1u << c);
        break;
    }
    case 6: { // heart
        static const uint8_t BIG[7] = {0x36,0x7F,0x7F,0x3E,0x1C,0x08,0x00};
        static const uint8_t SM[7]  = {0x00,0x36,0x3E,0x1C,0x08,0x00,0x00};
        const uint8_t *h = ((p / 6) & 1) ? BIG : SM;
        for (r = 0; r < 7; r++) fb[r] = h[r];
        break;
    }
    case 7: { // ball
        static int8_t bx = 0, by = 0, vx = 1, vy = 1;
        static uint32_t lastp = 0xFFFFFFFFu;
        if (p != lastp) {
            lastp = p;
            if ((p & 1u) == 0) {
                bx += vx; by += vy;
                if (bx < 0) { bx = 0; vx = 1; }
                if (bx > 6) { bx = 6; vx = -1; }
                if (by < 0) { by = 0; vy = 1; }
                if (by > 6) { by = 6; vy = -1; }
            }
        }
        fb[by] |= (uint8_t)(1u << bx);
        break;
    }
    case 8: { // chevron
        for (c = 0; c < 7; c++) {
            int x = (int)((p + c) % 6);
            uint8_t col = (x < 5) ? CHEV[x] : 0;
            for (r = 0; r < 7; r++) if (col & (1u << r)) fb[r] |= (uint8_t)(1u << c);
        }
        break;
    }
    case 9: { // spinner
        static const int8_t DR[8] = {0,1,1,1,0,-1,-1,-1};
        static const int8_t DC[8] = {1,1,0,-1,-1,-1,0,1};
        int a = (int)(p % 8);
        int rr = 3, cc = 3, k;
        for (k = 0; k < 3; k++) {
            rr += DR[a]; cc += DC[a];
            if (rr < 0 || rr > 6 || cc < 0 || cc > 6) break;
            fb[rr] |= (uint8_t)(1u << cc);
        }
        fb[3] |= (uint8_t)(1u << 3);
        break;
    }
    case 10: // checker
        for (r = 0; r < 7; r++)
            for (c = 0; c < 7; c++)
                if (((r + c + (uint8_t)(p / 2)) & 1) == 0) fb[r] |= (uint8_t)(1u << c);
        break;
    case 11: { // box
        int s = (int)(p % 9);
        int half = s / 2;
        int lo = 3 - half, hi = 3 + half;
        if (lo < 0) lo = 0; if (hi > 6) hi = 6;
        for (c = 0; c < 7; c++) { if (c < lo || c > hi) continue; fb[lo] |= (uint8_t)(1u << c); fb[hi] |= (uint8_t)(1u << c); }
        for (r = 0; r < 7; r++) { if (r < lo || r > hi) continue; fb[r] |= (uint8_t)(1u << lo); fb[r] |= (uint8_t)(1u << hi); }
        break;
    }
    case 12: // blink
        if (((p / 6) & 1) == 0)
            for (r = 0; r < 7; r++) fb[r] = 0x7F;
        break;
    }
    // g_sp is advanced in the main loop every SCROLL_MS (not here)
}

// ---------- main ----------
void setup() {
    RCC->APB2PCENR |= (1u << 2) | (1u << 4) | (1u << 5);
    blankAll();
    GPIOD->CFGLR = (GPIOD->CFGLR & ~NIB(7)) | (CFG_PU << 28);
    GPIOD->OUTDR |= (1u << 7);
    // PA1 (BTN2) not used - leave as default
}

void loop() {
    static uint8_t fx = 0;
    static uint32_t p = 0;
    static uint32_t last = 0;
    static uint32_t slast = 0;
    // g_sp is global (used by build_frame)
    static uint8_t prev1 = 1;
    static uint8_t asleep = 0;
    static uint8_t row = 0;
    static uint32_t tRow = 0;

    // BTN1 (PD7) = mode switch: short press = next effect, long press (>=500ms) = sleep
    uint8_t b1 = (GPIOD->INDR & (1u << 7)) ? 0 : 1;

    static uint32_t pressStart = 0;
    static uint8_t longTriggered = 0;
    static uint8_t btnState = 0; // 0=released, 1=pressed, 2=long-pressed

    if (b1 && !btnState) {
        btnState = 1;
        pressStart = millis();
    }
    if (b1 && btnState == 1) {
        if ((uint32_t)(millis() - pressStart) >= 500) {
            btnState = 2;
            if (!asleep) {
                asleep = 1;
                blankAll();
            }
        }
    }
    if (!b1 && btnState) {
        if (btnState == 1) {
            if (asleep) {
                asleep = 0;
                last = slast = millis();
            } else {
                fx = (uint8_t)((fx + 1) % FX_COUNT);
                p = 0; g_sp = 0;
                last = slast = millis();
            }
        }
        btnState = 0;
    }

    if (asleep) {
        blankAll();
        delay(20);
        return;
    }

    uint32_t nowUs = micros();
    if ((uint32_t)(nowUs - tRow) >= ROW_US) {
        tRow = nowUs;
        driveRow(row);
        if (++row >= 7) row = 0;
    }

    uint32_t ms = millis();
    static uint32_t lastFrame = 0;
    if ((uint32_t)(ms - lastFrame) >= STEP_MS) {
        lastFrame = ms;
        build_frame(fx, p);
        if ((uint32_t)(ms - last) >= STEP_MS) {
            last = ms;
            p++;
        }
    }
    // advance scroll position every SCROLL_MS (1px per step)
    if ((uint32_t)(ms - slast) >= SCROLL_MS) {
        slast = ms;
        g_sp++;
    }
}
