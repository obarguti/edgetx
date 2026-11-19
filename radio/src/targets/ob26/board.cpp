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

#include "stm32_adc.h"
#include "stm32_gpio.h"
#include "stm32_hal.h"

#include "board.h"

#include "hal/adc_driver.h"
#include "hal/switch_driver.h"
#include "hal/abnormal_reboot.h"
#include "hal/watchdog_driver.h"
#include "hal/usb_driver.h"
#include "hal/gpio.h"
#include "hal/rotary_encoder.h"

#include "globals.h"
#include "sdcard.h"
#include "debug.h"
#include "keys.h"

#include "timers_driver.h"
#include "delays_driver.h"

#include <string.h>

// common ADC driver
extern const etx_hal_adc_driver_t _adc_driver;

//
// Status LED
//
// The only LED on the core board (PH7) is fed from 3.3V through a resistor,
// so the pin has to be pulled low to light it.
//
void ledInit()
{
  gpio_init(LED_USER_GPIO, GPIO_OUT, GPIO_PIN_SPEED_LOW);
  gpio_set(LED_USER_GPIO);
}

static inline void ledOn() { gpio_clear(LED_USER_GPIO); }

void ledOff() { gpio_set(LED_USER_GPIO); }
void ledRed() { ledOn(); }
void ledGreen() { ledOn(); }
void ledBlue() { ledOn(); }

static void ledBlink(unsigned count, unsigned on_ms, unsigned off_ms)
{
  for (unsigned i = 0; i < count; i++) {
    ledOn();
    delay_ms(on_ms);
    ledOff();
    delay_ms(off_ms);
  }
}

//
// Bootloader hooks
//
void boardBLEarlyInit()
{
  ledInit();
  backlightLowInit();
  timersInit();
  delaysInit();
}

void boardBLPreJump()
{
  SDRAM_Init();

  // Bring-up check: 3 long blinks = SDRAM OK, endless fast blinking = SDRAM
  // failure. The firmware needs the SDRAM, so do not jump if it is broken.
  if (SDRAM_Test()) {
    ledBlink(3, 300, 200);
  } else {
    while (true) ledBlink(1, 80, 80);
  }

  // Stop 1ms timer
  MS_TIMER->CR1 &= ~TIM_CR1_CEN;
}

void boardBLInit()
{
  rotaryEncoderInit();
  SDRAM_Init();
}

//
// Firmware
//
void boardInit()
{
  // enable interrupts
  __enable_irq();

#if defined(DEBUG)
  serialSetMode(SP_AUX1, UART_MODE_DEBUG);                // indicate AUX1 is used
  serialInit(SP_AUX1, UART_MODE_DEBUG);                   // early AUX1 init
#endif

  ledInit();
  backlightLowInit();

  pwrInit();
  delaysInit();
  timersInit();
  rotaryEncoderInit();

  usbInit();

  keysInit();
  switchInit();
  audioInit();

  adcInit(&_adc_driver);

  // No battery sensing yet: report a healthy 8.4V / 3.0V
  setAnalogValue(adcGetInputOffset(ADC_INPUT_VBAT), 840 * 2);
  setAnalogValue(adcGetInputOffset(ADC_INPUT_RTC_BAT), 300 * 2);
}

extern void rtcDisableBackupReg();

void boardOff()
{
  lcdOff();

  while (pwrPressed()) {
    WDG_RESET();
  }

  SysTick->CTRL = 0; // turn off systick

  rtcDisableBackupReg();

  pwrOff();

  // We reach here only in forced power situations, such as hw-debugging
  // with external power. Enter STM32 stop mode / deep-sleep
  // Code snippet from ST Nucleo PWR_EnterStopMode example
#define PDMode             0x00000000U
#if defined(PWR_CR1_MRUDS) && defined(PWR_CR1_LPUDS) && defined(PWR_CR1_FPDS)
  MODIFY_REG(PWR->CR1, (PWR_CR1_PDDS | PWR_CR1_LPDS | PWR_CR1_FPDS | PWR_CR1_LPUDS | PWR_CR1_MRUDS), PDMode);
#elif defined(PWR_CR_MRLVDS) && defined(PWR_CR_LPLVDS) && defined(PWR_CR_FPDS)
  MODIFY_REG(PWR->CR1, (PWR_CR1_PDDS | PWR_CR1_LPDS | PWR_CR1_FPDS | PWR_CR1_LPLVDS | PWR_CR1_MRLVDS), PDMode);
#endif /* PWR_CR_MRUDS && PWR_CR_LPUDS && PWR_CR_FPDS */

  /* Set SLEEPDEEP bit of Cortex System Control Register */
  SET_BIT(SCB->SCR, ((uint32_t)SCB_SCR_SLEEPDEEP_Msk));

  // To avoid HardFault at return address, end in an endless loop
  while (1) {
  }
}

#if !defined(BOOT)
// Firmware heartbeat on the user LED: 3 s on, 3 s off
void per5ms()
{
  static uint16_t ticks = 0;
  if (ticks == 0) {
    ledOn();
  } else if (ticks == 600) {
    ledOff();
  }
  if (++ticks >= 1200) ticks = 0;
}
#endif

// The core board has no VBUS sense line (PA9 is the USART1 TX debug pin), so
// USB is never reported as plugged. Wire a divided VBUS to a free pin and
// read it here once USB mass storage / serial is needed.
int usbPlugged() { return 0; }
