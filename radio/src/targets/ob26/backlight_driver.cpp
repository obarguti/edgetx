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

// The shield's backlight is not switched by the MCU (PH6 is the blue LED),
// so these only track the requested state.
void backlightLowInit() {}

void backlightInit() {}

bool isBacklightEnabled() { return boardBacklightOn; }

void backlightFullOn() { boardBacklightOn = true; }

void backlightEnable(uint8_t dutyCycle) { boardBacklightOn = dutyCycle != 0; }
