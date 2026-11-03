#include <Arduino.h>

#define SPEED_MS 92
#define BRIGHTNESS 10

static const char* MSGS[] = {
  "Anh Th\xC6\xB0",
  "NTH",
};
#define MSG_COUNT ((int)(sizeof(MSGS) / sizeof(MSGS[0])))

#define NIB(p) (0xFu << ((p)*4))
#define CFG_OUT2 0x2u
#define CFG_PU 0x8u

static const uint8_t K_DPIN[6] = { 0, 2, 3, 4, 5, 6 };
#define D_CATH_MASK (NIB(0) | NIB(2) | NIB(3) | NIB(4) | NIB(5) | NIB(6))
#define D_CATH_FLT (0x44444444u & D_CATH_MASK)
#define D_OUT_MASK ((1u << 0) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6))

#define DEBOUNCE_MS 20
#define LMAX 15
#define PWM_STEPS 15
#define PWM_US 16

#define SCROLL_GAP 5
#define RIPPLE_PERIOD 10

#define NON_SCROLL_FX 9
#define TOTAL_STEPS (MSG_COUNT + NON_SCROLL_FX)

static uint8_t fb[7];
static uint8_t px[7][7];
static uint8_t disp[7][7];
static uint16_t g_msg_cols[MSG_COUNT];

static const uint8_t FACE_HAPPY[7] = { 0x00, 0x22, 0x55, 0x00, 0x22, 0x1C, 0x00 };
static const uint8_t FACE_HAPPY_BLINK[7] = { 0x00, 0x00, 0x77, 0x00, 0x22, 0x1C, 0x00 };
static const uint8_t FACE_ANGRY[7] = { 0x00, 0x41, 0x22, 0x41, 0x00, 0x22, 0x1C };
static const uint8_t FACE_ANGRY_BLINK[7] = { 0x00, 0x00, 0x77, 0x00, 0x00, 0x22, 0x1C };

static uint32_t face_blink_at = 0;
static uint32_t face_blink_end = 0;
static uint32_t face_rng = 0x1D2C3B4Au;

static uint32_t face_rand() {
  face_rng ^= face_rng << 13;
  face_rng ^= face_rng >> 17;
  face_rng ^= face_rng << 5;
  return face_rng;
}

static void allOff() {
  GPIOC->OUTDR = 0x007Fu;
  GPIOD->OUTDR &= ~D_OUT_MASK;
}

static void renderFrame() {
  for (uint8_t r = 0; r < 7; r++) {
    uint32_t rowC = 0x7Fu & ~(1u << r);
    for (uint8_t s = 0; s < PWM_STEPS; s++) {
      uint32_t outC = rowC;
      if (disp[r][0] > s) outC |= (1u << 7);
      uint32_t outD = 0;
      for (uint8_t c = 1; c < 7; c++)
        if (disp[r][c] > s) outD |= (1u << K_DPIN[c - 1]);
      GPIOC->OUTDR = outC;
      GPIOD->OUTDR = (GPIOD->OUTDR & ~D_OUT_MASK) | outD;
      delayMicroseconds(PWM_US);
    }
    GPIOC->OUTDR = 0x007Fu;
    GPIOD->OUTDR &= ~D_OUT_MASK;
  }
}

static void blendFrame() {
  for (uint8_t r = 0; r < 7; r++)
    for (uint8_t c = 0; c < 7; c++) {
      uint8_t tgt = (fb[r] & (1u << c)) ? LMAX : 0;
      px[r][c] = (uint8_t)((px[r][c] + 3u * tgt + 2u) >> 2);
      disp[r][c] = (uint8_t)(((uint16_t)px[r][c] * BRIGHTNESS + 7u) / 15u);
    }
}

static const uint8_t FONT[95][5] = {
  { 0x00, 0x00, 0x00, 0x00, 0x00 },
  { 0x00, 0x00, 0x5F, 0x00, 0x00 },
  { 0x00, 0x07, 0x00, 0x07, 0x00 },
  { 0x14, 0x7F, 0x14, 0x7F, 0x14 },
  { 0x24, 0x2A, 0x7F, 0x2A, 0x12 },
  { 0x23, 0x13, 0x08, 0x64, 0x62 },
  { 0x36, 0x49, 0x56, 0x20, 0x50 },
  { 0x00, 0x00, 0x07, 0x00, 0x00 },
  { 0x00, 0x1C, 0x22, 0x41, 0x00 },
  { 0x00, 0x41, 0x22, 0x1C, 0x00 },
  { 0x14, 0x08, 0x3E, 0x08, 0x14 },
  { 0x08, 0x08, 0x3E, 0x08, 0x08 },
  { 0x00, 0x40, 0x30, 0x00, 0x00 },
  { 0x08, 0x08, 0x08, 0x08, 0x08 },
  { 0x00, 0x00, 0x60, 0x60, 0x00 },
  { 0x40, 0x30, 0x08, 0x06, 0x01 },
  { 0x3E, 0x51, 0x49, 0x45, 0x3E },
  { 0x00, 0x42, 0x7F, 0x40, 0x00 },
  { 0x72, 0x49, 0x49, 0x49, 0x46 },
  { 0x21, 0x41, 0x49, 0x4D, 0x33 },
  { 0x18, 0x14, 0x12, 0x7F, 0x10 },
  { 0x27, 0x45, 0x45, 0x45, 0x39 },
  { 0x3C, 0x4A, 0x49, 0x49, 0x31 },
  { 0x41, 0x21, 0x11, 0x09, 0x07 },
  { 0x36, 0x49, 0x49, 0x49, 0x36 },
  { 0x46, 0x49, 0x49, 0x29, 0x1E },
  { 0x00, 0x00, 0x24, 0x00, 0x00 },
  { 0x00, 0x40, 0x34, 0x00, 0x00 },
  { 0x00, 0x08, 0x14, 0x22, 0x41 },
  { 0x14, 0x14, 0x14, 0x14, 0x14 },
  { 0x00, 0x41, 0x22, 0x14, 0x08 },
  { 0x02, 0x01, 0x59, 0x09, 0x06 },
  { 0x3E, 0x41, 0x5D, 0x59, 0x4E },
  { 0x7C, 0x12, 0x11, 0x12, 0x7C },
  { 0x7F, 0x49, 0x49, 0x49, 0x36 },
  { 0x3E, 0x41, 0x41, 0x41, 0x22 },
  { 0x7F, 0x41, 0x41, 0x41, 0x3E },
  { 0x7F, 0x49, 0x49, 0x49, 0x41 },
  { 0x7F, 0x09, 0x09, 0x09, 0x01 },
  { 0x3E, 0x41, 0x41, 0x51, 0x73 },
  { 0x7F, 0x08, 0x08, 0x08, 0x7F },
  { 0x00, 0x41, 0x7F, 0x41, 0x00 },
  { 0x20, 0x40, 0x41, 0x3F, 0x01 },
  { 0x7F, 0x08, 0x14, 0x22, 0x41 },
  { 0x7F, 0x40, 0x40, 0x40, 0x40 },
  { 0x7F, 0x02, 0x1C, 0x02, 0x7F },
  { 0x7F, 0x04, 0x08, 0x10, 0x7F },
  { 0x3E, 0x41, 0x41, 0x41, 0x3E },
  { 0x7F, 0x09, 0x09, 0x09, 0x06 },
  { 0x3E, 0x41, 0x51, 0x21, 0x5E },
  { 0x7F, 0x09, 0x19, 0x29, 0x46 },
  { 0x26, 0x49, 0x49, 0x49, 0x32 },
  { 0x03, 0x01, 0x7F, 0x01, 0x03 },
  { 0x3F, 0x40, 0x40, 0x40, 0x3F },
  { 0x1F, 0x20, 0x40, 0x20, 0x1F },
  { 0x3F, 0x40, 0x38, 0x40, 0x3F },
  { 0x63, 0x14, 0x08, 0x14, 0x63 },
  { 0x03, 0x04, 0x78, 0x04, 0x03 },
  { 0x61, 0x59, 0x49, 0x4D, 0x43 },
  { 0x00, 0x7F, 0x41, 0x41, 0x41 },
  { 0x01, 0x06, 0x08, 0x30, 0x40 },
  { 0x00, 0x41, 0x41, 0x41, 0x7F },
  { 0x04, 0x02, 0x01, 0x02, 0x04 },
  { 0x40, 0x40, 0x40, 0x40, 0x40 },
  { 0x00, 0x01, 0x02, 0x00, 0x00 },
  { 0x20, 0x54, 0x54, 0x78, 0x40 },
  { 0x7F, 0x28, 0x44, 0x44, 0x38 },
  { 0x38, 0x44, 0x44, 0x44, 0x28 },
  { 0x38, 0x44, 0x44, 0x28, 0x7F },
  { 0x38, 0x54, 0x54, 0x54, 0x18 },
  { 0x00, 0x08, 0x7E, 0x09, 0x02 },
  { 0x0C, 0x52, 0x52, 0x52, 0x3E },
  { 0x7F, 0x08, 0x04, 0x04, 0x78 },
  { 0x00, 0x44, 0x7D, 0x40, 0x00 },
  { 0x20, 0x40, 0x40, 0x3D, 0x00 },
  { 0x7F, 0x10, 0x28, 0x44, 0x00 },
  { 0x00, 0x41, 0x7F, 0x40, 0x00 },
  { 0x7C, 0x04, 0x78, 0x04, 0x78 },
  { 0x7C, 0x08, 0x04, 0x04, 0x78 },
  { 0x38, 0x44, 0x44, 0x44, 0x38 },
  { 0x7E, 0x12, 0x12, 0x12, 0x0C },
  { 0x0C, 0x12, 0x12, 0x12, 0x7E },
  { 0x7C, 0x08, 0x04, 0x04, 0x08 },
  { 0x48, 0x54, 0x54, 0x54, 0x24 },
  { 0x04, 0x04, 0x3F, 0x44, 0x24 },
  { 0x3C, 0x40, 0x40, 0x20, 0x7C },
  { 0x1C, 0x20, 0x40, 0x20, 0x1C },
  { 0x3C, 0x40, 0x30, 0x40, 0x3C },
  { 0x44, 0x28, 0x10, 0x28, 0x44 },
  { 0x0E, 0x50, 0x50, 0x50, 0x3E },
  { 0x44, 0x64, 0x54, 0x4C, 0x44 },
  { 0x00, 0x08, 0x36, 0x41, 0x00 },
  { 0x00, 0x00, 0x7F, 0x00, 0x00 },
  { 0x00, 0x41, 0x36, 0x08, 0x00 },
  { 0x08, 0x04, 0x08, 0x10, 0x08 },
};

enum { M_ACUTE, M_GRAVE, M_CIRC, M_BREVE, M_TILDE, M_HORN, M_STROKE };
static const uint8_t MARKS[7][5] = {
  { 0x00, 0x00, 0x02, 0x01, 0x00 },
  { 0x00, 0x01, 0x02, 0x00, 0x00 },
  { 0x00, 0x02, 0x01, 0x02, 0x00 },
  { 0x00, 0x01, 0x02, 0x01, 0x00 },
  { 0x02, 0x01, 0x02, 0x01, 0x00 },
  { 0x00, 0x00, 0x00, 0x00, 0x06 },
  { 0x00, 0x00, 0x02, 0x02, 0x00 },
};

struct Accent {
  uint16_t cp;
  char base;
  uint8_t mark;
};

static const Accent ACC[] = {
  { 0xE0, 'a', M_GRAVE }, { 0xE1, 'a', M_ACUTE }, { 0xE2, 'a', M_CIRC }, { 0xE3, 'a', M_TILDE },
  { 0xE8, 'e', M_GRAVE }, { 0xE9, 'e', M_ACUTE }, { 0xEA, 'e', M_CIRC },
  { 0xEC, 'i', M_GRAVE }, { 0xED, 'i', M_ACUTE },
  { 0xF2, 'o', M_GRAVE }, { 0xF3, 'o', M_ACUTE }, { 0xF4, 'o', M_CIRC }, { 0xF5, 'o', M_TILDE },
  { 0xF9, 'u', M_GRAVE }, { 0xFA, 'u', M_ACUTE },
  { 0x103, 'a', M_BREVE }, { 0x111, 'd', M_STROKE }, { 0x129, 'i', M_TILDE },
  { 0x169, 'u', M_TILDE }, { 0x1A1, 'o', M_HORN },
};

static const char CAPS_FALLBACK[30] = {
  'A', 'A', 'A', 'A', 'A', 'A', 'A', 'C', 'E', 'E', 'E', 'E', 'I', 'I', 'I',
  'I', 'D', 'N', 'O', 'O', 'O', 'O', 'O', 'x', 'O', 'U', 'U', 'U', 'U', 'Y'
};

struct CustomGlyph {
  uint16_t cp;
  uint8_t cols[7];
};

static const CustomGlyph CUSTOM_GLYPHS[] = {
  { 0x1B0, { 0x3C, 0x40, 0x40, 0x20, 0x7C, 0x02, 0x00 } },
};

#define SPACE_W 2

static int utf8_decode(const char* s, int i, uint32_t* cp) {
  uint8_t b = (uint8_t)s[i];
  if (b == 0) {
    *cp = 0;
    return 0;
  }
  if (b < 0x80) {
    *cp = b;
    return 1;
  }
  if ((b & 0xE0) == 0xC0) {
    *cp = ((uint32_t)(b & 0x1F) << 6) | ((uint8_t)s[i + 1] & 0x3F);
    return 2;
  }
  if ((b & 0xF0) == 0xE0) {
    *cp = ((uint32_t)(b & 0x0F) << 12) | (((uint8_t)s[i + 1] & 0x3F) << 6) | ((uint8_t)s[i + 2] & 0x3F);
    return 3;
  }
  *cp = b;
  return 1;
}

static uint8_t glyph_finish(uint8_t* g, uint8_t* lead) {
  uint8_t lo = 0, hi = 6;
  while (lo < 7 && g[lo] == 0) lo++;
  if (lo == 7) {
    *lead = 0;
    return SPACE_W;
  }
  while (g[hi] == 0) hi--;
  *lead = lo;
  return (uint8_t)(hi - lo + 1);
}

static uint8_t glyph_load(uint32_t cp, uint8_t* g, uint8_t* lead) {
  for (uint8_t i = 0; i < (uint8_t)(sizeof(CUSTOM_GLYPHS) / sizeof(CUSTOM_GLYPHS[0])); i++) {
    if (CUSTOM_GLYPHS[i].cp == cp) {
      for (uint8_t k = 0; k < 7; k++) g[k] = CUSTOM_GLYPHS[i].cols[k];
      return glyph_finish(g, lead);
    }
  }

  char base = '?';
  int8_t mark = -1;

  if (cp >= 0x20 && cp <= 0x7E) {
    base = (char)cp;
  } else if (cp >= 0xC0 && cp <= 0xDD) {
    base = CAPS_FALLBACK[cp - 0xC0];
  } else {
    for (uint8_t i = 0; i < (uint8_t)(sizeof(ACC) / sizeof(ACC[0])); i++) {
      if (ACC[i].cp == cp) {
        base = ACC[i].base;
        mark = (int8_t)ACC[i].mark;
        break;
      }
    }
  }

  for (uint8_t k = 0; k < 5; k++) {
    uint8_t v = FONT[base - 0x20][k];
    if (mark >= 0) {
      if (base == 'i') v &= (uint8_t)~1u;
      v |= MARKS[mark][k];
    }
    g[k] = v;
  }
  g[5] = 0;
  g[6] = 0;
  return glyph_finish(g, lead);
}

static int str_width(const char* s) {
  int i = 0, total = 0;
  uint32_t cp;
  uint8_t g[7], lead;
  while (s[i]) {
    int l = utf8_decode(s, i, &cp);
    if (l == 0) break;
    i += l;
    total += glyph_load(cp, g, &lead) + 1;
  }
  return total;
}

static uint8_t str_col(const char* s, int total, int x) {
  if (x < 0 || x >= total) return 0;
  int i = 0;
  uint32_t cp;
  uint8_t g[7], lead;
  while (s[i]) {
    int l = utf8_decode(s, i, &cp);
    if (l == 0) break;
    i += l;
    uint8_t w = glyph_load(cp, g, &lead);
    if (x < w) return g[lead + x];
    x -= w + 1;
    if (x < 0) return 0;
  }
  return 0;
}

static const int8_t WAVE[16] = { 0, 1, 2, 2, 3, 2, 2, 1, 0, -1, -2, -2, -3, -2, -2, -1 };

static void face_render(const uint8_t* normal, const uint8_t* blink, uint32_t p) {
  if (p == 0) {
    face_rng = 0x1D2C3B4Au ^ millis();
    face_blink_end = 0;
    face_blink_at = 12 + (face_rand() % 36);
  }
  if (p >= face_blink_at && p >= face_blink_end) {
    face_blink_end = p + 2;
    face_blink_at = p + 12 + (face_rand() % 36);
  }
  uint8_t blinking = (p < face_blink_end) ? 1 : 0;
  const uint8_t* f = blinking ? blink : normal;
  for (uint8_t r = 0; r < 7; r++) fb[r] = f[r];
}

static void build_frame(uint8_t step, uint32_t p) {
  uint8_t r, c;
  for (r = 0; r < 7; r++) fb[r] = 0;

  if (step < MSG_COUNT) {
    int total = (int)g_msg_cols[step];
    uint32_t period = (uint32_t)total + SCROLL_GAP;
    uint32_t pos = p % period;
    for (c = 0; c < 7; c++) {
      uint8_t col = str_col(MSGS[step], total, (int)((pos + c) % period));
      for (r = 0; r < 7; r++)
        if (col & (1u << r)) fb[r] |= (uint8_t)(1u << c);
    }
    return;
  }

  uint8_t fx = (uint8_t)(step - MSG_COUNT);
  switch (fx) {
    case 0:
      {
        for (c = 0; c < 7; c++) {
          int row = 3 + WAVE[(c + (int)p) & 15];
          if (row >= 0 && row < 7) fb[row] |= (uint8_t)(1u << c);
          if (row + 1 >= 0 && row + 1 < 7) fb[row + 1] |= (uint8_t)(1u << c);
        }
        break;
      }
    case 1:
      {
        for (c = 0; c < 7; c++) {
          int drop = (int)((p / 2 + (uint32_t)(c * 5)) % 12) - 4;
          for (int k = 0; k < 3; k++) {
            int rr = drop - k;
            if (rr >= 0 && rr < 7) fb[rr] |= (uint8_t)(1u << c);
          }
        }
        break;
      }
    case 2:
      {
        static const uint8_t BIG[7] = { 0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08, 0x00 };
        static const uint8_t SM[7] = { 0x00, 0x36, 0x3E, 0x1C, 0x08, 0x00, 0x00 };
        const uint8_t* h = ((p / 6) & 1) ? BIG : SM;
        for (r = 0; r < 7; r++) fb[r] = h[r];
        break;
      }
    case 3: face_render(FACE_HAPPY, FACE_HAPPY_BLINK, p); break;
    case 4: face_render(FACE_ANGRY, FACE_ANGRY_BLINK, p); break;
    case 5:
      {
        for (r = 0; r < 7; r++) {
          for (c = 0; c < 7; c++) {
            int dr = (int)r - 3, dc = (int)c - 3;
            int d2 = dr * dr + dc * dc;
            int d = 0;
            while ((d + 1) * (d + 1) <= d2) d++;
            int phase = ((int)(p % RIPPLE_PERIOD) + RIPPLE_PERIOD - d * 2) % RIPPLE_PERIOD;
            if (phase < 4) fb[r] |= (uint8_t)(1u << c);
          }
        }
        break;
      }
    case 6:
      {
        for (r = 0; r < 7; r++) {
          for (c = 0; c < 7; c++) {
            uint8_t h = (uint8_t)((((uint16_t)r * 7u) + c) * 37u + 13u);
            uint32_t ph = (p + h) % 23u;
            uint8_t v = ph < 12u ? (uint8_t)ph : (uint8_t)(23u - ph);
            if (v > 7u) fb[r] |= (uint8_t)(1u << c);
          }
        }
        break;
      }
    case 7:
      {
        int back = (int)(p % 10u);
        for (r = 0; r < 7; r++) {
          for (c = 0; c < 7; c++) {
            int ac = (c < 3) ? (3 - c) : (c - 3);
            int v = ((int)r + ac + 10 - back) % 10;
            if (v < 3) fb[r] |= (uint8_t)(1u << c);
          }
        }
        break;
      }
    case 8:
      {
        int period = 12;
        int px_ = (int)(p % period);
        int py_ = (int)((p + 3u) % period);
        int bx = px_ < 7 ? px_ : period - px_;
        int by = py_ < 7 ? py_ : period - py_;
        for (int dr = 0; dr < 2; dr++) {
          for (int dc = 0; dc < 2; dc++) {
            int rr = by + dr, cc = bx + dc;
            if (rr < 7 && cc < 7) fb[rr] |= (uint8_t)(1u << cc);
          }
        }
        break;
      }
  }
}

void setup() {
  RCC->APB2PCENR |= (1u << 0) | (1u << 2) | (1u << 4) | (1u << 5);
  SystemCoreClock = 24000000UL;
  SysTick->CTLR = 0;
  SysTick->CNT = 0;
  SysTick->CMP = 24000000UL / 1000UL - 1UL;
  SysTick->SR = 0;
  SysTick->CTLR = 0xF;

  AFIO->PCFR1 &= ~(1u << 15);

  GPIOC->CFGLR = 0x22222222u;
  GPIOD->CFGLR = (GPIOD->CFGLR & ~D_CATH_MASK) | (0x22222222u & D_CATH_MASK);
  GPIOD->CFGLR = (GPIOD->CFGLR & ~NIB(7)) | (CFG_PU << 28);
  GPIOD->OUTDR |= (1u << 7);

  GPIOA->CFGLR = (GPIOA->CFGLR & ~NIB(1)) | (CFG_PU << 4);
  GPIOA->OUTDR |= (1u << 1);

  for (int i = 0; i < MSG_COUNT; i++)
    g_msg_cols[i] = (uint16_t)str_width(MSGS[i]);

  allOff();
}

void loop() {
  static uint8_t step = 0;
  static uint32_t p = 0;
  static uint32_t lastBlend = 0;
  static uint32_t lastFrame = 0;
  static uint8_t asleep = 0;
  static uint8_t b1prev = 0, b2prev = 0;
  static uint32_t b1lock = 0, b2lock = 0;

  uint32_t ms = millis();
  uint8_t b1 = (GPIOD->INDR & (1u << 7)) ? 0 : 1;
  uint8_t b2 = (GPIOA->INDR & (1u << 1)) ? 0 : 1;

  if (b2 && !b2prev && (uint32_t)(ms - b2lock) >= DEBOUNCE_MS) {
    b2lock = ms;
    asleep = !asleep;
    if (asleep) allOff();
    else {
      lastFrame = ms;
      lastBlend = ms;
    }
  }
  b2prev = b2;

  if (b1 && !b1prev && (uint32_t)(ms - b1lock) >= DEBOUNCE_MS) {
    b1lock = ms;
    if (!asleep) {
      step = (uint8_t)((step + 1) % TOTAL_STEPS);
      p = 0;
      lastFrame = ms - SPEED_MS;
      lastBlend = ms - 15u;
    }
  }
  b1prev = b1;

  if (asleep) {
    allOff();
    delay(20);
    return;
  }

  if ((uint32_t)(ms - lastFrame) >= SPEED_MS) {
    lastFrame = ms;
    build_frame(step, p);
    p++;
  }

  if ((uint32_t)(ms - lastBlend) >= 15u) {
    lastBlend = ms;
    blendFrame();
  }

  renderFrame();
}