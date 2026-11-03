/*
 * 7x7 multiplexed LED keychain - CH32V003 (Arduino core)
 *
 * Wiring (from the board notes):
 *   Anodes   A1..A7 = PC0..PC6
 *   Cathodes K1..K7 = PC7, PD0, PD2, PD3, PD4, PD5, PD6
 *   BTN1 (next effect) = PD7   (NRST already disabled via option byte)
 *   BTN2 (sleep/wake)  = PA1
 *   Charge status      = PA2   (TP4056, active low)
 *   PD1 = SWIO (never touched)
 *
 * POLARITY: LEDs are soldered backwards, so a lit LED needs
 *   anode line LOW + cathode line HIGH. Idle lines float (hi-Z input).
 *
 * CURRENT: no series resistors! Every matrix line is configured through
 * the registers with the 2 MHz output speed. Do NOT use pinMode()/10/50 MHz.
 *
 * Scan: row scan, one anode LOW at a time, 250 us per row (~570 Hz frame).
 *
 * Power behaviour:
 *   - 30 s without a button press -> matrix dims (shorter row on-time).
 *   - Charger detected (PA2 low)  -> all LEDs off; any button wakes them.
 *     While still charging, the 30 s idle timeout turns them off again.
 *   - BTN2 = manual sleep, only BTN2 wakes it.
 */

#include <Arduino.h>

// ---------- GPIO register helpers (CFGLR nibble = CNF[1:0] MODE[1:0]) ----------
#define NIB(p)    (0xFu << ((p) * 4))
#define CFG_FLT   0x4u   // input, floating
#define CFG_OUT2  0x2u   // push-pull output, 2 MHz (the ONLY drive mode used on the matrix)
#define CFG_PU    0x8u   // input with pull (OUTDR bit = 1 -> pull-up)

// Port D pins used as cathodes K2..K7 (PD1 = SWIO, PD7 = BTN1 are excluded)
static const uint8_t K_DPIN[6] = {0, 2, 3, 4, 5, 6};
#define D_CATH_MASK (NIB(0) | NIB(2) | NIB(3) | NIB(4) | NIB(5) | NIB(6))
#define D_CATH_FLT  (0x44444444u & D_CATH_MASK)
#define D_OUT_MASK  ((1u<<0) | (1u<<2) | (1u<<3) | (1u<<4) | (1u<<5) | (1u<<6))

// ---------- timing ----------
#define ROW_US 250UL

// ---------- frame buffers: one byte per row, bit c = column c ----------
static uint8_t fb[7];    // what the scanner displays
static uint8_t buf[7];   // canvas the effects draw on

// forward declaration: Arduino's sketch preprocessor inserts generated
// function prototypes before the first function, and pressed() takes a Btn&,
// so the type name must be known before that insertion point.
struct Btn;

static inline void present() { for (uint8_t i = 0; i < 7; i++) fb[i] = buf[i]; }
static inline void clearBuf() { for (uint8_t i = 0; i < 7; i++) buf[i] = 0; }
static inline void setPx(uint8_t r, uint8_t c) { buf[r] |= (uint8_t)(1u << c); }

// ---------- low-level matrix drive ----------
static void blankAll() {
  GPIOC->CFGLR = 0x44444444u;                                   // all PC floating
  GPIOD->CFGLR = (GPIOD->CFGLR & ~D_CATH_MASK) | D_CATH_FLT;    // cathodes floating, PD1/PD7 kept
}

static void driveRow(uint8_t r) {
  uint8_t cols = fb[r];

  uint32_t cfgC = 0x44444444u, outC = 0;
  uint32_t cfgD = D_CATH_FLT,  outD = 0;

  // anode line of this row: output, LOW (OUTDR bit stays 0)
  cfgC = (cfgC & ~NIB(r)) | (CFG_OUT2 << (r * 4));

  // lit cathodes: output, HIGH
  for (uint8_t c = 0; c < 7; c++) {
    if (!(cols & (1u << c))) continue;
    if (c == 0) {                       // K1 = PC7
      cfgC = (cfgC & ~NIB(7)) | (CFG_OUT2 << 28);
      outC |= (1u << 7);
    } else {                            // K2..K7 = PD0,2,3,4,5,6
      uint8_t p = K_DPIN[c - 1];
      cfgD = (cfgD & ~NIB(p)) | (CFG_OUT2 << (p * 4));
      outD |= (1u << p);
    }
  }

  blankAll();                           // break-before-make between rows
  if (!cols) return;                    // nothing lit in this row

  GPIOC->OUTDR = outC;
  GPIOD->OUTDR = (GPIOD->OUTDR & ~D_OUT_MASK) | outD;           // keeps PD7 pull-up bit
  GPIOC->CFGLR = cfgC;
  GPIOD->CFGLR = (GPIOD->CFGLR & ~D_CATH_MASK) | cfgD;
}

// ---------- buttons (active low, software debounce) ----------
static inline bool btn1Raw() { return !(GPIOD->INDR & (1u << 7)); }
static inline bool btn2Raw() { return !(GPIOA->INDR & (1u << 1)); }
static inline bool charging() { return !(GPIOA->INDR & (1u << 2)); }  // TP4056 CHRG, active low

struct Btn { bool (*raw)(); bool state; bool lastRaw; uint32_t t; };
static Btn b1 = { btn1Raw, false, false, 0 };
static Btn b2 = { btn2Raw, false, false, 0 };

// returns true once per debounced press
static bool pressed(Btn &b, uint32_t ms) {
  bool r = b.raw();
  if (r != b.lastRaw) { b.lastRaw = r; b.t = ms; }
  if ((ms - b.t) >= 25 && r != b.state) {
    b.state = r;
    if (r) return true;
  }
  return false;
}

// ---------- tiny PRNG ----------
static uint32_t rs = 2463534242u;
static uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

// ---------- 5x7 font (column-major, bit0 = top row) ----------
// order: 0-9, A-Z, '!', '-', ' '
static const uint8_t FONT[39][5] = {
  {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},
  {0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
  {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
  {0x06,0x49,0x49,0x29,0x1E},
  {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
  {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
  {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
  {0x00,0x00,0x5F,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x00,0x00,0x00,0x00}
};

static const char TEXT[] = "HELLO  ";   // <- change your scrolling message here (A-Z 0-9 ! - space)

static uint8_t textColumn(int16_t i) {
  if (i < 0) return 0;
  uint16_t ch = i / 6, k = i % 6;
  if (ch >= sizeof(TEXT) - 1 || k == 5) return 0;
  char ct = TEXT[ch];
  uint8_t g;
  if (ct >= '0' && ct <= '9')      g = ct - '0';
  else if (ct >= 'A' && ct <= 'Z') g = 10 + (ct - 'A');
  else if (ct >= 'a' && ct <= 'z') g = 10 + (ct - 'a');
  else if (ct == '!')              g = 36;
  else if (ct == '-')              g = 37;
  else                             g = 38;
  return FONT[g][k];
}

// ---------- spiral path (row-major LED indices) ----------
static uint8_t spiral[49];
static void buildSpiral() {
  int8_t top = 0, bot = 6, left = 0, right = 6;
  uint8_t k = 0;
  while (top <= bot && left <= right) {
    for (int8_t c = left; c <= right; c++) spiral[k++] = top * 7 + c;
    top++;
    for (int8_t r = top; r <= bot; r++) spiral[k++] = r * 7 + right;
    right--;
    if (top <= bot) { for (int8_t c = right; c >= left; c--) spiral[k++] = bot * 7 + c; bot--; }
    if (left <= right) { for (int8_t r = bot; r >= top; r--) spiral[k++] = r * 7 + left; left++; }
  }
}

// ---------- effects ----------
#define NUM_FX 7
static const uint16_t FX_MS[NUM_FX] = { 80, 100, 110, 30, 50, 70, 450 };
static uint8_t  fx = 0;
static uint16_t st = 0;
static uint32_t tFx = 0;

static void effectReset() { clearBuf(); present(); st = 0; tFx = millis(); }

static void effectStep() {
  switch (fx) {
    case 0: {                                   // scrolling text
      uint16_t total = (sizeof(TEXT) - 1) * 6;
      clearBuf();
      for (uint8_t c = 0; c < 7; c++) {
        uint8_t colBits = textColumn((int16_t)st + c - 7);
        for (uint8_t r = 0; r < 7; r++) if (colBits & (1u << r)) setPx(r, c);
      }
      if (++st > total + 7) st = 0;
      break;
    }
    case 1:                                     // rain
      for (uint8_t r = 6; r > 0; r--) buf[r] = buf[r - 1];
      buf[0] = (rnd() % 3 == 0) ? (uint8_t)(1u << (rnd() % 7)) : 0;
      break;

    case 2: {                                   // ripple from the centre
      clearBuf();
      uint8_t radius = st;
      for (uint8_t r = 0; r < 7; r++)
        for (uint8_t c = 0; c < 7; c++) {
          int8_t dr = (int8_t)r - 3, dc = (int8_t)c - 3;
          if (dr < 0) dr = -dr;
          if (dc < 0) dc = -dc;
          uint8_t d = (dr > dc) ? dr : dc;
          if (d == radius) setPx(r, c);
        }
      if (++st >= 4) st = 0;
      break;
    }
    case 3: {                                   // spiral snake with a tail
      clearBuf();
      for (uint8_t i = 0; i < 9; i++) {
        uint8_t idx = spiral[(st + 49 - i) % 49];
        setPx(idx / 7, idx % 7);
      }
      if (++st >= 49) st = 0;
      break;
    }
    case 4:                                     // sparkle (~1/3 of LEDs lit)
      buf[rnd() % 7] |=  (uint8_t)(1u << (rnd() % 7));
      buf[rnd() % 7] &= ~(uint8_t)(1u << (rnd() % 7));
      buf[rnd() % 7] &= ~(uint8_t)(1u << (rnd() % 7));
      break;

    case 5: {                                   // crossing scanner lines
      uint8_t pos = (st < 7) ? st : (12 - st);
      clearBuf();
      buf[pos] = 0x7F;
      for (uint8_t r = 0; r < 7; r++) buf[r] |= (uint8_t)(1u << pos);
      if (++st >= 12) st = 0;
      break;
    }
    case 6: {                                   // beating heart
      static const uint8_t big[7]   = {0x36,0x7F,0x7F,0x3E,0x1C,0x08,0x00};
      static const uint8_t small_[7]= {0x00,0x14,0x3E,0x1C,0x08,0x00,0x00};
      const uint8_t *src = st ? small_ : big;
      for (uint8_t r = 0; r < 7; r++) buf[r] = src[r];
      st ^= 1;
      break;
    }
  }
  present();
}

// ---------- power / dim state ----------
#define DIM_AFTER_MS     30000UL  // idle time before dimming
#define LIT_DIM_US       30UL     // row on-time when dimmed (normal = full 250 us slot)
#define CHG_DEBOUNCE_MS  300UL

static bool     dimmed    = false;
static bool     rowLit    = false;
static uint32_t lastAct   = 0;
static bool     chgRaw    = false, chgStable = false;
static uint32_t chgT      = 0;

// Debounced charge state; returns true once, when charging has just started.
static bool updateCharge(uint32_t ms) {
  bool r = charging();
  if (r != chgRaw) { chgRaw = r; chgT = ms; }
  if ((ms - chgT) >= CHG_DEBOUNCE_MS && r != chgStable) {
    chgStable = r;
    return r;
  }
  return false;
}

// Matrix completely off until a button is pressed.
// anyButton=false: only BTN2 wakes (manual sleep); true: either button wakes (charging).
static void powerOff(bool anyButton) {
  blankAll();
  clearBuf(); present();
  rowLit = false;
  for (;;) {
    delay(10);
    uint32_t ms = millis();
    updateCharge(ms);                 // keep tracking the charger while off
    bool p1 = pressed(b1, ms);
    bool p2 = pressed(b2, ms);
    if (p2 || (anyButton && p1)) break;
  }
  dimmed  = false;
  lastAct = millis();
  effectReset();
}

// ---------- Arduino entry points ----------
static uint8_t  row = 0;
static uint32_t tRow = 0;

void setup() {
  // clocks for GPIOA / GPIOC / GPIOD
  RCC->APB2PCENR |= (1u << 2) | (1u << 4) | (1u << 5);

  // PA1 (BTN2) and PA2 (charge status): input with pull-up
  GPIOA->CFGLR = (GPIOA->CFGLR & ~(NIB(1) | NIB(2))) | (CFG_PU << 4) | (CFG_PU << 8);
  GPIOA->OUTDR |= (1u << 1) | (1u << 2);

  // PD7 (BTN1): input with pull-up (NRST already disabled in the option byte)
  GPIOD->CFGLR = (GPIOD->CFGLR & ~NIB(7)) | (CFG_PU << 28);
  GPIOD->OUTDR |= (1u << 7);

  blankAll();
  buildSpiral();
  rs ^= micros();
  if (!rs) rs = 1;

  chgRaw = chgStable = charging();    // booting while plugged in is not a "charge start" edge
  chgT = millis();
  lastAct = millis();
  effectReset();
  tRow = micros();
}

void loop() {
  uint32_t nowUs = micros();
  if ((uint32_t)(nowUs - tRow) >= ROW_US) {
    tRow = nowUs;
    driveRow(row);
    rowLit = true;
    if (++row >= 7) row = 0;
  } else if (dimmed && rowLit && (uint32_t)(nowUs - tRow) >= LIT_DIM_US) {
    blankAll();                       // dimming = shorter on-time inside the same row slot
    rowLit = false;
  }

  uint32_t ms = millis();

  // When dimmed, effects advance in the dark part of the slot so they can't stretch a lit row.
  if ((!dimmed || !rowLit) && (uint32_t)(ms - tFx) >= FX_MS[fx]) {
    tFx = ms;
    effectStep();
  }

  // charger just plugged in -> everything off until a button press
  if (updateCharge(ms)) { powerOff(true); tRow = micros(); return; }

  bool p1 = pressed(b1, ms);
  bool p2 = pressed(b2, ms);
  if (p1 || p2) { lastAct = ms; dimmed = false; }   // any press restores full brightness

  if (p1) {                           // next effect
    rs ^= micros();
    fx = (fx + 1) % NUM_FX;
    effectReset();
  }
  if (p2) {                           // manual sleep
    powerOff(false);
    tRow = micros();
    return;
  }

  // idle timeout: dim, or switch off entirely if the charger is attached
  if (!dimmed && (uint32_t)(ms - lastAct) >= DIM_AFTER_MS) {
    if (chgStable) { powerOff(true); tRow = micros(); }
    else dimmed = true;
  }
}
