// SPDX-FileCopyrightText: Copyright (c) 2026 Adafruit Industries
// SPDX-License-Identifier: MIT
#include "MetroNeoPixel.h"

static inline uint32_t elapsedTicks(uint32_t start) {
  uint32_t now = SysTick->CNT;
  // Arduino's SysTick wraps every millisecond, including with IRQs masked.
  return now >= start ? now - start : now + SysTick->CMP + 1 - start;
}

void metroNeoPixelWrite(uint8_t red, uint8_t green, uint8_t blue) {
  metroNeoPixelWriteOnPin(PIN_NEOPIXEL, red, green, blue);
}

void metroNeoPixelWriteOnPin(uint16_t pin, uint8_t red, uint8_t green,
                             uint8_t blue) {
  PinName name = digitalPinToPinName(pin);
  if (name == NC)
    return;
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
  GPIO_TypeDef *port = set_GPIO_Port_Clock(CH_PORT(name));
  const uint32_t pinMask = CH_GPIO_PIN(name);
  delayMicroseconds(300);
  const uint8_t colors[] = {green, red, blue};
  // Budget for the GPIO write and polling-loop overhead. At the Metro's
  // 200 MHz clock, timer/DMA loopback measures the resulting pad waveform.
  const uint32_t ticksPerMicrosecond = F_CPU / 1000000;
  const uint32_t period = ticksPerMicrosecond * 1150 / 1000;
  const uint32_t zeroHigh = ticksPerMicrosecond * 250 / 1000;
  const uint32_t oneHigh = ticksPerMicrosecond * 700 / 1000;
  uint32_t status;
  // The vendor startup runs sketches in U mode; GINTENR is its IRQ CSR.
  const uint32_t mask = 0x88;
  __asm volatile("csrrc %0, 0x800, %1" : "=r"(status) : "r"(mask) : "memory");
  __asm volatile("fence.i" ::: "memory");
  for (uint8_t value : colors) {
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
      uint32_t highTicks = value & mask ? oneHigh : zeroHigh;
      port->BSHR = pinMask;
      uint32_t start = SysTick->CNT;
      while (elapsedTicks(start) < highTicks) {
      }
      port->BCR = pinMask;
      while (elapsedTicks(start) < period) {
      }
    }
  }
  __asm volatile("csrw 0x800, %0" : : "r"(status) : "memory");
}
