#include <Arduino.h>

void setup() {
  RCC->APB2PCENR |= (1u << 2) | (1u << 4) | (1u << 5);
  GPIOC->OUTDR &= ~(1u << 0);
  GPIOC->OUTDR |=  (1u << 7);
  GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu | (0xFu << 28))) | (0x2u | (0x2u << 28));
}

void loop() {
  static uint32_t last = 0;
  static bool on = true;
  uint32_t now = millis();
  if (now - last >= 500) {
    last = now;
    on = !on;
    if (on) {
      GPIOC->OUTDR &= ~(1u << 0);
      GPIOC->OUTDR |=  (1u << 7);
      GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu | (0xFu << 28))) | (0x2u | (0x2u << 28));
    } else {
      GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu | (0xFu << 28))) | (0x4u | (0x4u << 28));
    }
  }
}
