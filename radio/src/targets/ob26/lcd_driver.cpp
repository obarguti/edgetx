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
#include "hal/gpio.h"
#include "stm32_gpio.h"
#include "delays_driver.h"
#include "debug.h"
#include "lcd.h"
#include "board.h"

#include <string.h>

// 8-bit parallel (Intel 8080) ILI9486 driver, bit-banged over GPIO.
//
// Wiring (3.5" ILI9486 Arduino shield):
//   D0..D7 -> PI0..PI7  (low byte of GPIOI, written with one BSRR store)
//   RD  -> PD3 (kept high)   RST -> PD4   CS -> PD5   RS -> PD6   WR -> PD7
//
// The panel is run in landscape (MV) at 480x320, RGB565 (two bus cycles per
// pixel). EdgeTX hands over the whole frame buffer on every flush but only
// reports the last dirty area, so the driver diffs the frame against a shadow
// copy and only sends the rectangles that changed.

#define LCD_DATA_PORT     GPIOI
#define LCD_DATA_SHIFT    0
#define LCD_CTRL_PORT     GPIOD
#define LCD_RD_BIT        (1U << 3)
#define LCD_RST_BIT       (1U << 4)
#define LCD_CS_BIT        (1U << 5)
#define LCD_RS_BIT        (1U << 6)
#define LCD_WR_BIT        (1U << 7)

// WR low and high phase, in nanoseconds. Timed on the DWT cycle counter
// (enabled by delaysInit()): __NOP() loops are not reliable on the Cortex-M7.
// 500 ns is the value proven on the FK743 breakout with the hand-wired shield;
// lower it in steps (e.g. 250, 120) to find the fastest reliable setting.
#define LCD_WR_NS         500
#define LCD_WR_CYCLES     ((CPU_FREQ / 1000000U) * LCD_WR_NS / 1000U)

// MADCTL: MV (0x20) = landscape, BGR (0x08) = colour order.
// Landscape rotated by 180 degrees: 0xE8. Swapped red/blue: clear BGR.
#define LCD_MADCTL        0x28

const char* boardLcdType = "ILI9486 8-bit";

static void* initialFrameBuffer = nullptr;

static pixel_t _shadow[LCD_W * LCD_H] __SDRAM __ALIGNED(4);
static bool _shadowValid = false;

static inline void busDelay(uint32_t cycles)
{
  (void)LCD_CTRL_PORT->IDR;  // make sure the previous pin store reached the port
  uint32_t start = DWT->CYCCNT;
  while (DWT->CYCCNT - start < cycles) {
  }
}

static inline void busWrite(uint8_t b)
{
  LCD_DATA_PORT->BSRR =
      ((uint32_t)b << LCD_DATA_SHIFT) |
      ((uint32_t)(uint8_t)~b << (16 + LCD_DATA_SHIFT));
  LCD_CTRL_PORT->BSRR = (uint32_t)LCD_WR_BIT << 16;  // WR low
  busDelay(LCD_WR_CYCLES);
  LCD_CTRL_PORT->BSRR = LCD_WR_BIT;                   // WR high: data latched
  busDelay(LCD_WR_CYCLES);
}

static inline void lcdCmd(uint8_t c)
{
  LCD_CTRL_PORT->BSRR = (uint32_t)LCD_RS_BIT << 16;   // RS low = command
  busWrite(c);
  LCD_CTRL_PORT->BSRR = LCD_RS_BIT;                   // RS high = data
}

static inline void lcdData(uint8_t d) { busWrite(d); }

static void lcdCmdData(uint8_t c, const uint8_t* d, unsigned n)
{
  lcdCmd(c);
  for (unsigned i = 0; i < n; i++) lcdData(d[i]);
}

// Slow edges on purpose: fast edges ring on long jumper wires and
// double-clock WR, which leaves the panel white.
static void lcdGpioInit()
{
  for (int i = 0; i < 8; i++) {
    gpio_init(GPIO_PIN(LCD_DATA_PORT, (LCD_DATA_SHIFT + i)), GPIO_OUT,
              GPIO_PIN_SPEED_LOW);
  }
  for (int i = 3; i < 8; i++) {
    gpio_init(GPIO_PIN(LCD_CTRL_PORT, i), GPIO_OUT, GPIO_PIN_SPEED_LOW);
  }
  // idle: CS high, WR high, RD high, RS high, RST high
  LCD_CTRL_PORT->BSRR = LCD_WR_BIT | LCD_RS_BIT | LCD_CS_BIT | LCD_RD_BIT |
                        LCD_RST_BIT;
}

static inline void csLow() { LCD_CTRL_PORT->BSRR = (uint32_t)LCD_CS_BIT << 16; }
static inline void csHigh() { LCD_CTRL_PORT->BSRR = LCD_CS_BIT; }

static void lcdReset()
{
  LCD_CTRL_PORT->BSRR = LCD_RST_BIT;
  delay_ms(5);
  LCD_CTRL_PORT->BSRR = (uint32_t)LCD_RST_BIT << 16;
  delay_ms(20);
  LCD_CTRL_PORT->BSRR = LCD_RST_BIT;
  delay_ms(150);
}

// Init sequence verified on the FK743 + 3.5" ILI9486 shield (ili9486_fk743.cpp)
static void lcdPanelInit()
{
  csLow();

  lcdCmd(0x01);                       // software reset
  delay_ms(150);
  lcdCmd(0x11);                       // sleep out
  delay_ms(150);

  { const uint8_t d[] = {0x55}; lcdCmdData(0x3A, d, 1); }              // 16 bit / pixel
  { const uint8_t d[] = {0x0D, 0x0D}; lcdCmdData(0xC0, d, 2); }        // power control 1
  { const uint8_t d[] = {0x43, 0x00}; lcdCmdData(0xC1, d, 2); }        // power control 2
  { const uint8_t d[] = {0x00}; lcdCmdData(0xC2, d, 1); }              // power control 3
  { const uint8_t d[] = {0x00, 0x48, 0x00, 0x48}; lcdCmdData(0xC5, d, 4); }  // VCOM
  { const uint8_t d[] = {0x02, 0x02, 0x3B}; lcdCmdData(0xB6, d, 3); }  // display function
  { const uint8_t d[] = {LCD_MADCTL}; lcdCmdData(0x36, d, 1); }

  lcdCmd(0x20);                       // display inversion off
  lcdCmd(0x29);                       // display on
  delay_ms(50);

  csHigh();
}

// Set the drawing window and start a memory write.
static void lcdSetWindow(unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{
  uint8_t d[4];

  d[0] = x0 >> 8; d[1] = x0 & 0xFF; d[2] = x1 >> 8; d[3] = x1 & 0xFF;
  lcdCmdData(0x2A, d, 4);

  d[0] = y0 >> 8; d[1] = y0 & 0xFF; d[2] = y1 >> 8; d[3] = y1 & 0xFF;
  lcdCmdData(0x2B, d, 4);

  lcdCmd(0x2C);
}

static inline bool rowEqual(const pixel_t* a, const pixel_t* b)
{
  return memcmp(a, b, LCD_W * sizeof(pixel_t)) == 0;
}

static void startLcdRefresh(lv_disp_drv_t *disp_drv, uint16_t *buffer,
                            const rect_t &copy_area)
{
  (void)disp_drv;
  (void)copy_area;

  const pixel_t* frame = (const pixel_t*)buffer;

  csLow();

  unsigned y = 0;
  while (y < LCD_H) {
    const pixel_t* row = frame + y * LCD_W;
    pixel_t* shadow = _shadow + y * LCD_W;

    if (_shadowValid && rowEqual(row, shadow)) {
      y++;
      continue;
    }

    // Changed block: extend until a few identical rows in a row, so that
    // small gaps do not cost a new window setup each.
    unsigned y0 = y, y1 = y, same = 0;
    while (++y < LCD_H) {
      if (_shadowValid &&
          rowEqual(frame + y * LCD_W, _shadow + y * LCD_W)) {
        if (++same >= 4) break;
      } else {
        same = 0;
        y1 = y;
      }
    }

    // Narrow the block to the changed columns.
    unsigned xmin = 0, xmax = LCD_W - 1;
    if (_shadowValid) {
      xmin = LCD_W - 1;
      xmax = 0;
      for (unsigned r = y0; r <= y1; r++) {
        const pixel_t* a = frame + r * LCD_W;
        const pixel_t* b = _shadow + r * LCD_W;
        unsigned l = 0;
        while (l < LCD_W && a[l] == b[l]) l++;
        if (l == LCD_W) continue;
        unsigned h = LCD_W - 1;
        while (a[h] == b[h]) h--;
        if (l < xmin) xmin = l;
        if (h > xmax) xmax = h;
      }
    }

    lcdSetWindow(xmin, y0, xmax, y1);
    for (unsigned r = y0; r <= y1; r++) {
      const pixel_t* a = frame + r * LCD_W;
      pixel_t* b = _shadow + r * LCD_W;
      for (unsigned x = xmin; x <= xmax; x++) {
        pixel_t p = a[x];
        busWrite(p >> 8);
        busWrite(p & 0xFF);
        b[x] = p;
      }
    }
  }

  csHigh();
  _shadowValid = true;

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
  lcdGpioInit();
  lcdReset();
  lcdPanelInit();

  _shadowValid = false;
  TRACE("LCD INIT: ILI9486 8-bit parallel");
  lcdSetFlushCb(startLcdRefresh);
}

extern "C" void lcdOff()
{
  csLow();
  lcdCmd(0x28);
  csHigh();
}

extern "C" void lcdOn()
{
  csLow();
  lcdCmd(0x29);
  csHigh();
}
