#include <Arduino.h>

void setup() {
  RCC->APB2PCENR |= (1u << 2) | (1u << 4) | (1u << 5);
  // D1 = row0/col0: anode PC0 LOW, cathode PC7 HIGH (reversed polarity)
  GPIOC->OUTDR &= ~(1u << 0);
  GPIOC->OUTDR |=  (1u << 7);
  GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu | (0xFu << 28))) | (0x2u | (0x2u << 28));
}

void loop() {}
