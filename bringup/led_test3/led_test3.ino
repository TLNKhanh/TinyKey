#include <Arduino.h>

void setup() {
  RCC->APB2PCENR |= (1u << 2) | (1u << 4) | (1u << 5);
  GPIOC->OUTDR &= ~(1u << 0);
  GPIOC->OUTDR |=  (1u << 7);
  GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu | (0xFu << 28))) | (0x2u | (0x2u << 28));
}

void loop() {
  delay(500);
  GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu | (0xFu << 28))) | (0x4u | (0x4u << 28));
  delay(500);
  GPIOC->OUTDR &= ~(1u << 0);
  GPIOC->OUTDR |=  (1u << 7);
  GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu | (0xFu << 28))) | (0x2u | (0x2u << 28));
}
