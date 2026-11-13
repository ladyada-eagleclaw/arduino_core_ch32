// SPDX-FileCopyrightText: Copyright (c) 2026 Adafruit Industries
// SPDX-License-Identifier: BSD-3-Clause
#pragma once
#include "ch32v4x7_gpio.h"
#include "ch32v4x7_rcc.h"

enum {
  AFIO_NONE,
  AFIO_USART1_DEFAULT,
  AFIO_USART1_PB6_PB7,
  AFIO_SPI1_DEFAULT,
  AFIO_SPI3_DEFAULT,
  AFIO_TIM1_DEFAULT,
  AFIO_TIM2_DEFAULT,
  AFIO_TIM3_DEFAULT,
  AFIO_TIM4_DEFAULT,
  AFIO_I2C1_PB8_PB9,
};

static inline void pinV32_DisconnectDebug(PinName pin) {
  // Debug pads are not exposed by the Metro variant.
  (void)pin;
}

static inline void pin_SetV32AFPin(uint32_t afnum) {
  RCC_PB2PeriphClockCmd(RCC_PB2Periph_AFIO, ENABLE);
  switch (afnum) {
  case AFIO_USART1_DEFAULT:
    GPIO_PinRemapConfig(GPIO_FullRemap_USART1, DISABLE);
    break;
  case AFIO_USART1_PB6_PB7:
    GPIO_PinRemapConfig(GPIO_PartialRemap1_USART1, ENABLE);
    break;
  case AFIO_SPI1_DEFAULT:
    GPIO_PinRemapConfig(GPIO_Remap_SPI1, DISABLE);
    break;
  case AFIO_SPI3_DEFAULT:
    GPIO_PinRemapConfig(GPIO_Remap_SPI3, DISABLE);
    break;
  case AFIO_TIM1_DEFAULT:
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM1, DISABLE);
    break;
  case AFIO_TIM2_DEFAULT:
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM2, DISABLE);
    break;
  case AFIO_TIM3_DEFAULT:
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, DISABLE);
    break;
  case AFIO_TIM4_DEFAULT:
    GPIO_PinRemapConfig(GPIO_Remap_TIM4, DISABLE);
    break;
  case AFIO_I2C1_PB8_PB9:
    GPIO_PinRemapConfig(GPIO_Remap_I2C1, ENABLE);
    break;
  default:
    break;
  }
}
