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
#include "stm32_hal_ll.h"

#include "board.h"

// FK743M2-IIT6: Winbond W9825G6 (32 MB, 16 bit, 4 banks, 13 row / 9 column
// bits) on FMC SDRAM bank 1 (SDNE0 / SDCKE0), mapped at 0xC0000000.
// SDCLK = HCLK / 2 = 100 MHz (see system_clock.c).

#define SDRAM_BASE_ADDR   0xC0000000UL
#define SDRAM_SIZE_BYTES  (32UL * 1024 * 1024)

#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

#define GPIO_AF_FMC GPIO_AF12

static const gpio_t _sdram_gpios[] = {
  // D0..D15
  GPIO_PIN(GPIOD, 14), GPIO_PIN(GPIOD, 15), GPIO_PIN(GPIOD, 0),
  GPIO_PIN(GPIOD, 1),  GPIO_PIN(GPIOE, 7),  GPIO_PIN(GPIOE, 8),
  GPIO_PIN(GPIOE, 9),  GPIO_PIN(GPIOE, 10), GPIO_PIN(GPIOE, 11),
  GPIO_PIN(GPIOE, 12), GPIO_PIN(GPIOE, 13), GPIO_PIN(GPIOE, 14),
  GPIO_PIN(GPIOE, 15), GPIO_PIN(GPIOD, 8),  GPIO_PIN(GPIOD, 9),
  GPIO_PIN(GPIOD, 10),
  // A0..A12
  GPIO_PIN(GPIOF, 0),  GPIO_PIN(GPIOF, 1),  GPIO_PIN(GPIOF, 2),
  GPIO_PIN(GPIOF, 3),  GPIO_PIN(GPIOF, 4),  GPIO_PIN(GPIOF, 5),
  GPIO_PIN(GPIOF, 12), GPIO_PIN(GPIOF, 13), GPIO_PIN(GPIOF, 14),
  GPIO_PIN(GPIOF, 15), GPIO_PIN(GPIOG, 0),  GPIO_PIN(GPIOG, 1),
  GPIO_PIN(GPIOG, 2),
  // BA0, BA1
  GPIO_PIN(GPIOG, 4),  GPIO_PIN(GPIOG, 5),
  // NBL0, NBL1
  GPIO_PIN(GPIOE, 0),  GPIO_PIN(GPIOE, 1),
  // SDCLK, SDNCAS, SDNRAS, SDNWE, SDNE0, SDCKE0
  GPIO_PIN(GPIOG, 8),  GPIO_PIN(GPIOG, 15), GPIO_PIN(GPIOF, 11),
  GPIO_PIN(GPIOH, 5),  GPIO_PIN(GPIOH, 3),  GPIO_PIN(GPIOH, 2),
};

extern "C" void SDRAM_GPIOConfig(void)
{
  for (unsigned i = 0; i < DIM(_sdram_gpios); i++) {
    gpio_init_af(_sdram_gpios[i], GPIO_AF_FMC, GPIO_PIN_SPEED_VERY_HIGH);
  }
}

extern "C" void SDRAM_InitSequence(void)
{
  FMC_SDRAM_CommandTypeDef cmd;

  /* Clock configuration enable command */
  cmd.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
  cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  cmd.AutoRefreshNumber = 1;
  cmd.ModeRegisterDefinition = 0;
  FMC_SDRAM_SendCommand(FMC_Bank5_6_R, &cmd, 10);

  /* Datasheet requires >= 100 us after the clock is stable */
  delay_ms(2);

  /* Precharge all */
  cmd.CommandMode = FMC_SDRAM_CMD_PALL;
  cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  cmd.AutoRefreshNumber = 1;
  cmd.ModeRegisterDefinition = 0;
  FMC_SDRAM_SendCommand(FMC_Bank5_6_R, &cmd, 10);

  /* Auto-refresh x8 */
  cmd.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  cmd.AutoRefreshNumber = 8;
  cmd.ModeRegisterDefinition = 0;
  FMC_SDRAM_SendCommand(FMC_Bank5_6_R, &cmd, 10);

  /* Program the mode register: BL=1, sequential, CL=3, single write */
  cmd.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
  cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  cmd.AutoRefreshNumber = 1;
  cmd.ModeRegisterDefinition =
      (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1 |
      SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
      SDRAM_MODEREG_CAS_LATENCY_3 |
      SDRAM_MODEREG_OPERATING_MODE_STANDARD |
      SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;
  FMC_SDRAM_SendCommand(FMC_Bank5_6_R, &cmd, 10);

  /* Refresh rate: 64 ms / 8192 rows = 7.81 us -> 781 SDCLK @ 100 MHz,
     minus 20 as required by the reference manual. */
  FMC_SDRAM_ProgramRefreshRate(FMC_Bank5_6_R, 761);
}

extern "C" void SDRAM_Init(void)
{
  // delay function needed
  delaysInit();

  /* GPIO configuration for FMC SDRAM bank */
  SDRAM_GPIOConfig();

  /* Enable FMC clock */
  LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_FMC);

  FMC_SDRAM_InitTypeDef init;
  FMC_SDRAM_TimingTypeDef timing;

  /* SDCLK period = 10 ns */
  timing.LoadToActiveDelay    = 2;  // tMRD = 2 clk
  timing.ExitSelfRefreshDelay = 8;  // tXSR = 72 ns
  timing.SelfRefreshTime      = 5;  // tRAS = 42 ns min
  timing.RowCycleDelay        = 6;  // tRC  = 60 ns
  timing.WriteRecoveryTime    = 2;  // tWR  = 2 clk
  timing.RPDelay              = 2;  // tRP  = 18 ns
  timing.RCDDelay             = 2;  // tRCD = 18 ns

  init.SDBank             = FMC_SDRAM_BANK1;
  init.ColumnBitsNumber   = FMC_SDRAM_COLUMN_BITS_NUM_9;
  init.RowBitsNumber      = FMC_SDRAM_ROW_BITS_NUM_13;
  init.MemoryDataWidth    = FMC_SDRAM_MEM_BUS_WIDTH_16;
  init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  init.CASLatency         = FMC_SDRAM_CAS_LATENCY_3;
  init.WriteProtection    = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  init.SDClockPeriod      = FMC_SDRAM_CLOCK_PERIOD_2;
  init.ReadBurst          = FMC_SDRAM_RBURST_ENABLE;
  init.ReadPipeDelay      = FMC_SDRAM_RPIPE_DELAY_1;

  FMC_SDRAM_Init(FMC_Bank5_6_R, &init);
  FMC_SDRAM_Timing_Init(FMC_Bank5_6_R, &timing, FMC_SDRAM_BANK1);

  __FMC_ENABLE();

  SDRAM_InitSequence();
}

// Quick data / address line check, intended for board bring-up.
// Uses power-of-two offsets so a stuck or shorted address line aliases
// two locations and shows up as a mismatch.
extern "C" bool SDRAM_Test(void)
{
  volatile uint32_t* base = (volatile uint32_t*)SDRAM_BASE_ADDR;
  const uint32_t words = SDRAM_SIZE_BYTES / sizeof(uint32_t);

  // Data bus: walking ones / zeros on a single location
  for (uint32_t bit = 0; bit < 32; bit++) {
    base[0] = (1UL << bit);
    if (base[0] != (1UL << bit)) return false;
    base[0] = ~(1UL << bit);
    if (base[0] != ~(1UL << bit)) return false;
  }

  // Address bus: write a unique value at every power-of-two offset...
  for (uint32_t off = 1; off < words; off <<= 1) {
    base[off] = 0xA5000000UL | off;
  }
  base[0] = 0xA5000000UL;

  // ...then check none of them were overwritten by an aliased write.
  for (uint32_t off = 1; off < words; off <<= 1) {
    if (base[off] != (0xA5000000UL | off)) return false;
  }
  if (base[0] != 0xA5000000UL) return false;

  return true;
}
