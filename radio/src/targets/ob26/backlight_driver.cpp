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

#include "hal/gpio.h"
#include "stm32_gpio.h"

#include "board.h"

bool boardBacklightOn = false;

// The backlight is a plain on/off GPIO until the real panel is wired up.
void backlightLowInit()
{
  gpio_init(LCD_BACKLIGHT_GPIO, GPIO_OUT, GPIO_PIN_SPEED_LOW);
  gpio_clear(LCD_BACKLIGHT_GPIO);
}

void backlightInit() { backlightLowInit(); }

bool isBacklightEnabled() { return boardBacklightOn; }

void backlightFullOn()
{
  boardBacklightOn = true;
  gpio_set(LCD_BACKLIGHT_GPIO);
}

void backlightEnable(uint8_t dutyCycle)
{
  if (dutyCycle) {
    gpio_set(LCD_BACKLIGHT_GPIO);
  } else {
    gpio_clear(LCD_BACKLIGHT_GPIO);
  }
}
