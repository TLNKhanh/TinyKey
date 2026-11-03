#include <Arduino.h>

#define NIB(p)    (0xFu << ((p) * 4))
#define CFG_FLT   0x4u
#define CFG_OUT2  0x2u
#define CFG_PU    0x8u

static const uint8_t K_DPIN[6] = {0, 2, 3, 4, 5, 6};
#define D_CATH_MASK (NIB(0) | NIB(2) | NIB(3) | NIB(4) | NIB(5) | NIB(6))
#define D_CATH_FLT  (0x44444444u & D_CATH_MASK)
#define D_OUT_MASK  ((1u<<0) | (1u<<2) | (1u<<3) | (1u<<4) | (1u<<5) | (1u<<6))

static uint8_t fb[7] = {0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F};  // all LEDs on

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
    if (c == 0) {
      cfgC = (cfgC & ~NIB(7)) | (CFG_OUT2 << 28);
      outC |= (1u << 7);
    } else {
      uint8_t p = K_DPIN[c - 1];
      cfgD = (cfgD & ~NIB(p)) | (CFG_OUT2 << (p * 4));
      outD |= (1u << p);
    }
  }

  blankAll();
  if (!cols) return;

  GPIOC->OUTDR = outC;
  GPIOD->OUTDR = (GPIOD->OUTDR & ~D_OUT_MASK) | outD;
  GPIOC->CFGLR = cfgC;
  GPIOD->CFGLR = (GPIOD->CFGLR & ~D_CATH_MASK) | cfgD;
}

void setup() {
  RCC->APB2PCENR |= (1u << 2) | (1u << 4) | (1u << 5);
  blankAll();
}

void loop() {
  static uint8_t row = 0;
  static uint32_t tRow = 0;
  uint32_t nowUs = micros();
  if ((uint32_t)(nowUs - tRow) >= 250) {
    tRow = nowUs;
    driveRow(row);
    if (++row >= 7) row = 0;
  }
}
