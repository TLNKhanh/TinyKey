#include <Arduino.h>

#define NIB(p)      (0xFu << ((p) * 4))
#define CFG_PU      0x8u

static const uint8_t K_DPIN[6] = {0, 2, 3, 4, 5, 6};
#define D_CATH_MASK (NIB(0) | NIB(2) | NIB(3) | NIB(4) | NIB(5) | NIB(6))
#define D_OUT_MASK  ((1u << 0) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6))

#define STEP_MS      85
#define DEBOUNCE_MS  20

#define BRIGHT       7
#define PWM_STEPS    15
#define PWM_US       16

static const char *const TEXTS[] = {
    "Nam Khanh",
};
#define TEXT_COUNT ((uint8_t)(sizeof(TEXTS) / sizeof(TEXTS[0])))

enum { FX_TEXT, FX_WAVE, FX_RIPPLE, FX_RAIN, FX_HEART, FX_BALL, FX_CHEVRON, FX_FACE, FX_COUNT };

static uint8_t fb[7];
static uint8_t px[7][7];

static const char FONT_CHARS[] = " !'-.,:?0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static const uint8_t FONT[][5] = {
    {0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x5F,0x00,0x00},
    {0x00,0x00,0x03,0x00,0x00},
    {0x00,0x08,0x08,0x08,0x00},
    {0x00,0x60,0x60,0x00,0x00},
    {0x00,0x60,0x20,0x00,0x00},
    {0x00,0x36,0x36,0x00,0x00},
    {0x02,0x01,0x51,0x09,0x06},
    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x41,0x49,0x49,0x49,0x36},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},
    {0x7E,0x09,0x09,0x09,0x7E},
    {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x41,0x3E},
    {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x3A},
    {0x7F,0x08,0x08,0x08,0x7F},
    {0x41,0x41,0x7F,0x41,0x41},
    {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x02,0x04,0x08,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x41,0x21,0x1E},
    {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},
    {0x7F,0x20,0x10,0x20,0x7F},
    {0x63,0x14,0x08,0x14,0x63},
    {0x03,0x04,0x78,0x04,0x03},
    {0x61,0x51,0x49,0x45,0x43},
    {0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x44,0x44,0x44,0x38},
    {0x38,0x44,0x44,0x44,0x44},
    {0x38,0x44,0x44,0x44,0x7F},
    {0x38,0x54,0x54,0x54,0x18},
    {0x04,0x7E,0x05,0x05,0x00},
    {0x4C,0x52,0x52,0x12,0x3E},
    {0x7F,0x04,0x04,0x04,0x78},
    {0x00,0x44,0x7D,0x40,0x00},
    {0x40,0x40,0x40,0x3D,0x00},
    {0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00},
    {0x7C,0x04,0x78,0x04,0x7C},
    {0x7C,0x04,0x04,0x04,0x78},
    {0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x24,0x24,0x24,0x18},
    {0x18,0x24,0x24,0x24,0x7C},
    {0x7C,0x04,0x04,0x04,0x08},
    {0x48,0x54,0x54,0x54,0x24},
    {0x04,0x3F,0x44,0x44,0x00},
    {0x3C,0x40,0x40,0x40,0x7C},
    {0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},
    {0x0C,0x50,0x20,0x10,0x0C},
    {0x44,0x64,0x54,0x4C,0x44},
    {0x20,0x55,0x56,0x54,0x78},
};
#define FONT_AGRAVE 70

static const uint8_t FACE[][7] = {
    {0x00,0x36,0x36,0x00,0x22,0x1C,0x00},
    {0x00,0x36,0x36,0x00,0x1C,0x14,0x1C},
    {0x00,0x00,0x36,0x00,0x1C,0x14,0x00},
    {0x00,0x06,0x36,0x00,0x22,0x1C,0x00},
    {0x00,0x36,0x36,0x00,0x3E,0x22,0x1C},
    {0x00,0x22,0x77,0x22,0x00,0x22,0x1C},
    {0x00,0x55,0x77,0x00,0x11,0x15,0x0A},
};
#define FACE_COUNT ((uint8_t)(sizeof(FACE) / sizeof(FACE[0])))

static uint32_t hash32(uint32_t x) { x ^= x >> 13; x *= 1274126177u; x ^= x >> 16; return x; }

static const int8_t WAVE[16] = {0,1,2,2,3,2,2,1,0,-1,-2,-2,-3,-2,-2,-1};

static void allOff() {
  GPIOC->OUTDR = 0x007Fu;
  GPIOD->OUTDR &= ~D_OUT_MASK;
}

static void renderFrame() {
  for (uint8_t r = 0; r < 7; r++) {
    uint32_t rowC = 0x7Fu & ~(1u << r);
    for (uint8_t s = 0; s < PWM_STEPS; s++) {
      uint32_t outC = rowC;
      if (px[r][0] > s) outC |= (1u << 7);
      uint32_t outD = 0;
      for (uint8_t c = 1; c < 7; c++)
        if (px[r][c] > s) outD |= (1u << K_DPIN[c - 1]);
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
      uint8_t tgt = (fb[r] & (1u << c)) ? BRIGHT : 0;
      px[r][c] = (uint8_t)((px[r][c] + tgt + 1u) >> 1);
    }
}

static int font_index(char ch) {
  for (int i = 0; FONT_CHARS[i]; i++) if (FONT_CHARS[i] == ch) return i;
  return 7;
}

static int text_len(const char *s) {
  const uint8_t *p = (const uint8_t *)s;
  int n = 0;
  while (*p) {
    if ((*p & 0xE0u) == 0xC0u && (p[1] & 0xC0u) == 0x80u) p++;
    p++;
    n++;
  }
  return n;
}

static uint8_t text_col(const char *s, int i) {
  if (i < 0) return 0;
  int ch = i / 6;
  int k = i % 6;
  if (k == 5) return 0;
  const uint8_t *p = (const uint8_t *)s;
  for (int n = 0; *p; n++) {
    if (n == ch) {
      if (*p == 0xC3u && p[1] == 0xA0u) return FONT[FONT_AGRAVE][k];
      return FONT[font_index((char)*p)][k];
    }
    if ((*p & 0xE0u) == 0xC0u && (p[1] & 0xC0u) == 0x80u) p++;
    p++;
  }
  return 0;
}

static void build_frame(uint8_t fx, uint32_t p) {
  uint8_t r, c;
  for (r = 0; r < 7; r++) fb[r] = 0;

  switch (fx) {

  case FX_TEXT: {
    static uint8_t ti = 0;
    static uint16_t tx = 0;
    static uint32_t tp = 0xFFFFFFFFu;
    if (p != tp) {
      tp = p;
      if (p == 0) tx = 0;
      uint16_t len = (uint16_t)(text_len(TEXTS[ti]) * 6u);
      if (tx > (uint16_t)(len + 7u)) {
        tx = 0;
        ti = (uint8_t)((ti + 1u) % TEXT_COUNT);
      }
      tx++;
    }
    int base = (int)tx - 7;
    for (c = 0; c < 7; c++) {
      uint8_t bits = text_col(TEXTS[ti], base + (int)c);
      for (r = 0; r < 7; r++)
        if (bits & (1u << r)) fb[r] |= (uint8_t)(1u << c);
    }
    break;
  }

  case FX_WAVE:
    for (c = 0; c < 7; c++) {
      int row = 3 + WAVE[(c + (int)p) & 15];
      if (row < 7) fb[row] |= (uint8_t)(1u << c);
      if (row + 1 < 7) fb[row + 1] |= (uint8_t)(1u << c);
    }
    break;

  case FX_RIPPLE: {
    int phase = (int)((p >> 1) & 3u);
    for (r = 0; r < 7; r++)
      for (c = 0; c < 7; c++) {
        int dr = (r > 3) ? r - 3 : 3 - r;
        int dc = (c > 3) ? c - 3 : 3 - c;
        int d = (dr > dc) ? dr : dc;
        if (d == phase) fb[r] |= (uint8_t)(1u << c);
      }
    break;
  }

  case FX_RAIN:
    for (c = 0; c < 7; c++) {
      int drop = (int)((p / 2u + (uint32_t)(c * 5)) % 13u) - 3;
      for (int k = 0; k < 3; k++) {
        int rr = drop - k;
        if (rr >= 0 && rr < 7) fb[rr] |= (uint8_t)(1u << c);
      }
    }
    break;

  case FX_HEART: {
    static const uint8_t BIG[7] = {0x36,0x7F,0x7F,0x3E,0x1C,0x08,0x00};
    static const uint8_t SM[7]  = {0x00,0x36,0x7F,0x3E,0x1C,0x08,0x00};
    const uint8_t *h = ((p / 6u) & 1u) ? BIG : SM;
    for (r = 0; r < 7; r++) fb[r] = h[r];
    break;
  }

  case FX_BALL: {
    static int16_t bx = 8, by = 12;
    static int8_t vx = 2, vy = 3;
    static uint32_t bp = 0xFFFFFFFFu;
    if (p != bp) {
      bp = p;
      if (p == 0) { bx = 8; by = 12; vx = 2; vy = 3; }
      bx += vx;
      by += vy;
      if (bx < 0) { bx = 0; vx = (int8_t)-vx; }
      else if (bx > 24) { bx = 24; vx = (int8_t)-vx; }
      if (by < 0) { by = 0; vy = (int8_t)-vy; }
      else if (by > 24) { by = 24; vy = (int8_t)-vy; }
    }
    int rr = by >> 2;
    int cc = bx >> 2;
    fb[rr] |= (uint8_t)(1u << cc);
    int8_t tr = (vy > 0) ? -1 : 1;
    int8_t tc = (vx > 0) ? -1 : 1;
    for (int k = 1; k <= 2; k++) {
      int r2 = rr + tr * k;
      int c2 = cc + tc * k;
      if (r2 >= 0 && r2 < 7 && c2 >= 0 && c2 < 7) fb[r2] |= (uint8_t)(1u << c2);
    }
    break;
  }

  case FX_CHEVRON: {
    static const uint8_t CHEV[5] = {0x41,0x22,0x14,0x08,0x08};
    for (c = 0; c < 7; c++) {
      uint8_t x = (uint8_t)((p + c) % 6u);
      if (x >= 5) continue;
      for (r = 0; r < 7; r++)
        if (CHEV[x] & (1u << r)) fb[r] |= (uint8_t)(1u << c);
    }
    break;
  }

  case FX_FACE: {
    static uint8_t emo = 0, last = 0xFF, hold = 0;
    static uint8_t blinkWait = 12, blink = 0;
    static uint32_t seed = 0x2545F491u;
    static uint32_t fp = 0xFFFFFFFFu;
    if (p != fp) {
      fp = p;
      if (p == 0) { hold = 0; blink = 0; blinkWait = 12; last = 0xFF; }
      if (hold == 0) {
        seed = hash32(seed + p + 0x9E3779B9u);
        emo = (uint8_t)(seed % FACE_COUNT);
        if (emo == last) emo = (uint8_t)((emo + 1u) % FACE_COUNT);
        last = emo;
        hold = (uint8_t)(14u + ((seed >> 8) % 32u));
      } else hold--;
      if (blink) blink--;
      else if (blinkWait) blinkWait--;
      else {
        blink = 2;
        blinkWait = (uint8_t)(18u + (hash32(p + seed) % 32u));
      }
    }
    for (r = 0; r < 7; r++) fb[r] = FACE[emo][r];
    if (blink) { fb[1] = 0; fb[2] = 0x36; fb[3] = 0; }
    break;
  }

  }
}

void setup() {
  RCC->APB2PCENR |= (1u << 0) | (1u << 2) | (1u << 4) | (1u << 5);

  SystemCoreClock = 24000000UL;
  SysTick->CTLR = 0;
  SysTick->CNT  = 0;
  SysTick->CMP  = 24000000UL / 1000UL - 1UL;
  SysTick->SR   = 0;
  SysTick->CTLR = 0xF;

  AFIO->PCFR1 &= ~(1u << 15);

  GPIOC->CFGLR = 0x22222222u;
  GPIOD->CFGLR = (GPIOD->CFGLR & ~D_CATH_MASK) | (0x22222222u & D_CATH_MASK);
  GPIOD->CFGLR = (GPIOD->CFGLR & ~NIB(7)) | (CFG_PU << 28);
  GPIOD->OUTDR |= (1u << 7);

  GPIOA->CFGLR = (GPIOA->CFGLR & ~NIB(1)) | (CFG_PU << 4);
  GPIOA->OUTDR |= (1u << 1);

  allOff();
}

void loop() {
  static uint8_t fx = 0;
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
    else { lastFrame = ms; lastBlend = ms; }
  }
  b2prev = b2;

  if (b1 && !b1prev && (uint32_t)(ms - b1lock) >= DEBOUNCE_MS) {
    b1lock = ms;
    if (!asleep) {
      fx = (uint8_t)((fx + 1u) % FX_COUNT);
      p = 0;
      lastFrame = ms;
      lastBlend = ms;
    }
  }
  b1prev = b1;

  if (asleep) {
    allOff();
    delay(20);
    return;
  }

  renderFrame();

  if ((uint32_t)(ms - lastBlend) >= 15u) {
    lastBlend = ms;
    blendFrame();
  }

  if ((uint32_t)(ms - lastFrame) >= STEP_MS) {
    lastFrame = ms;
    build_frame(fx, p);
    p++;
  }
}
