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

#include "stm32_hal_ll.h"
#include "stm32_hal.h"
#include "edgetx_types.h"
#include "hal.h"
#include "debug.h"
#include "lcd.h"
#include "board.h"

// Placeholder display driver for first board bring-up.
//
// The panel (8-bit parallel ILI9486 shield) is not driven yet: frames are
// accepted and immediately reported as flushed so the UI keeps running.
// The real 8080 GPIO driver will replace startLcdRefresh().

const char* boardLcdType = "none";

static void* initialFrameBuffer = nullptr;

static void startLcdRefresh(lv_disp_drv_t *disp_drv, uint16_t *buffer,
                            const rect_t &copy_area)
{
  (void)disp_drv;
  (void)buffer;
  (void)copy_area;

  lcdFlushed();
}

extern "C"
void lcdSetInitalFrameBuffer(void* fbAddress)
{
  initialFrameBuffer = fbAddress;
}

extern "C"
void lcdInit(void)
{
  TRACE("LCD INIT: no panel driver (stub)");
  lcdSetFlushCb(startLcdRefresh);
}

extern "C" void lcdOff() {}
extern "C" void lcdOn() {}
