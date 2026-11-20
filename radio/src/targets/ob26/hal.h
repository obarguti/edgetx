/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*

OB26 on FK743M2-IIT6 (STM32H743IIT6)

DMA1
  (free)

DMA2
Stream0:  ADC_EXT_DMA_STREAM (do not move)
Stream1:  AUX_SERIAL_DMA_TX_STREAM
Stream2:  AUX_SERIAL_DMA_RX_STREAM
Stream4:  ADC_DMA_STREAM (do not move)

TIM12:  MIXER_SCHEDULER_TIMER
TIM14:  MS_TIMER
TIM7:   STATUS_LED_PWM_TIMER

USART1: AUX_SERIAL_USART (debug / CLI, PA9 TX / PA10 RX)

Pins used by the core board (do not reuse):
  FMC SDRAM:  PD0 PD1 PD8-PD10 PD14 PD15  PE0 PE1 PE7-PE15  PF0-PF5 PF11-PF15
              PG0 PG1 PG2 PG4 PG5 PG8 PG15  PH2 PH3 PH5
  QSPI flash: PB2 PB6 PD11 PD12 PD13 PE2
  SDMMC1:     PC8-PC12 PD2
  USB FS:     PA11 PA12
  SWD:        PA13 PA14
  User LED:   PH7
 */

#ifndef _HAL_H_
#define _HAL_H_

#include "hal_settings.h"

#define ADC_VREF_PREC2                  330

// User LED on the core board (PH7)
#define LED_USER_GPIO                   GPIO_PIN(GPIOH, 7)


// SD card (SDMMC1)
#define SD_SDIO                         SDMMC1
#define SD_SDIO_CLK_DIV(fq)             (HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC) / (2 * fq))
#define SD_SDIO_TRANSFER_CLK_DIV        SD_SDIO_CLK_DIV(24000000)    // Hz
#define STORAGE_USE_SDIO

// AUX1 serial port: USART1 (PA9 / PA10)
#define AUX_SERIAL_TX_GPIO                  GPIO_PIN(GPIOA, 9)
#define AUX_SERIAL_RX_GPIO                  GPIO_PIN(GPIOA, 10)
#define AUX_SERIAL_USART                    USART1
#define AUX_SERIAL_USART_IRQHandler         USART1_IRQHandler
#define AUX_SERIAL_USART_IRQn               USART1_IRQn
#define AUX_SERIAL_DMA_TX                   DMA2
#define AUX_SERIAL_DMA_TX_STREAM            LL_DMA_STREAM_1
#define AUX_SERIAL_DMA_TX_CHANNEL           LL_DMAMUX1_REQ_USART1_TX
#define AUX_SERIAL_DMA_RX                   DMA2
#define AUX_SERIAL_DMA_RX_STREAM            LL_DMA_STREAM_2
#define AUX_SERIAL_DMA_RX_CHANNEL           LL_DMAMUX1_REQ_USART1_RX

#endif // _HAL_H_
