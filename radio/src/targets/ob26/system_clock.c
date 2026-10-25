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

#include "stm32h7xx_ll_pwr.h"
#include "stm32h7xx_ll_rcc.h"
#include "stm32h7xx_ll_system.h"
#include "stm32h7xx_ll_bus.h"

#define BOOTSTRAP __attribute__((section(".bootstrap")))

// OB26 clock tree (FK743M2-IIT6: 25 MHz HSE, STM32H743, VOS1 / no overdrive)
//
//   HSE 25 MHz / M=5      =   5 MHz   PLL1 input
//   x N=160               = 800 MHz   PLL1 VCO
//   P=2                   = 400 MHz   SYSCLK
//   Q=8                   = 100 MHz   SDMMC kernel clock
//   HPRE /2               = 200 MHz   AHB / AXI / FMC (SDRAM clock = HCLK/2)
//   APB1/2/3/4 /2         = 100 MHz   (timers x2 = 200 MHz)
//
//   HSE 25 MHz / M=25     =   1 MHz   PLL3 input
//   x N=192               = 192 MHz   PLL3 VCO
//   Q=4                   =  48 MHz   USB kernel clock
BOOTSTRAP void SystemClock_Config(void)
{
  /* Power Configuration */
  LL_PWR_ConfigSupply(LL_PWR_LDO_SUPPLY);
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
  while (LL_PWR_IsActiveFlag_VOS() == 0){
  }

  /* Enable HSE oscillator */
  LL_RCC_HSE_Enable();
  while (LL_RCC_HSE_IsReady() != 1) {
  }

  /* Flash: 3 wait states / programming delay 2 for 200 MHz AXI at VOS1 */
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_3);
  MODIFY_REG(FLASH->ACR, FLASH_ACR_WRHIGHFREQ, 2U << FLASH_ACR_WRHIGHFREQ_Pos);

  /* Main PLL configuration and activation */
  LL_RCC_PLL_SetSource(LL_RCC_PLLSOURCE_HSE);
  LL_RCC_PLL1P_Enable();
  LL_RCC_PLL1Q_Enable();
  LL_RCC_PLL1R_Enable();
  LL_RCC_PLL1FRACN_Disable();
  LL_RCC_PLL1_SetVCOInputRange(LL_RCC_PLLINPUTRANGE_4_8);
  LL_RCC_PLL1_SetVCOOutputRange(LL_RCC_PLLVCORANGE_WIDE);
  LL_RCC_PLL1_SetM(5);
  LL_RCC_PLL1_SetN(160);
  LL_RCC_PLL1_SetP(2);
  LL_RCC_PLL1_SetQ(8);
  LL_RCC_PLL1_SetR(2);
  LL_RCC_PLL1_Enable();
  while (LL_RCC_PLL1_IsReady() != 1) {
  }

  /* Set Sys & AHB & APB1 & APB2 & APB3 & APB4 prescaler */
  LL_RCC_SetSysPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_2);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
  LL_RCC_SetAPB3Prescaler(LL_RCC_APB3_DIV_2);
  LL_RCC_SetAPB4Prescaler(LL_RCC_APB4_DIV_2);

  /* Set PLL1 as System Clock Source */
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL1);
  while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL1) {
  }

  /* Config & activate PLL3 (USB 48 MHz) */
  LL_RCC_PLL3Q_Enable();
  LL_RCC_PLL3FRACN_Disable();
  LL_RCC_PLL3_SetVCOInputRange(LL_RCC_PLLINPUTRANGE_1_2);
  LL_RCC_PLL3_SetVCOOutputRange(LL_RCC_PLLVCORANGE_MEDIUM);
  LL_RCC_PLL3_SetM(25);
  LL_RCC_PLL3_SetN(192);
  LL_RCC_PLL3_SetP(2);
  LL_RCC_PLL3_SetQ(4);
  LL_RCC_PLL3_SetR(2);
  LL_RCC_PLL3_Enable();
  while (LL_RCC_PLL3_IsReady() != 1) {
  }

  /* Enable SRAM1, SRAM2 & SRAM3 */
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_D2SRAM1 |
                           LL_AHB2_GRP1_PERIPH_D2SRAM2 |
                           LL_AHB2_GRP1_PERIPH_D2SRAM3);

  /* Set periph clock sources */
  LL_RCC_SetSDMMCClockSource(LL_RCC_SDMMC_CLKSOURCE_PLL1Q);
  LL_RCC_SetUSBClockSource(LL_RCC_USB_CLKSOURCE_PLL3Q);
  LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_CLKP);
}
