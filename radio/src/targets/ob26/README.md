# OB26 — custom STM32H743 EdgeTX radio

EdgeTX target for a custom transmitter built from a **FANK FK743M2-IIT6** core board (STM32H743IIT6) on a custom PCB, driving a 3.5" **ILI9486** 8-bit parallel shield. Branch: `ob26-bringup`.

Hardware: PCB revision **V1.0** (git tag `V1.0`). One board has been fabricated, with no hand modifications. The core board plugs into two 2x28 headers, P2 and P3. Changes from the earlier schematic: the nRF24 header was removed, the RGB LED became common cathode, battery sense moved from PC2 to PA4, and the PAGE button (SW13, PH4) was added.

The firmware is the source of truth for pin assignments: [hal.h](hal.h) and [../../boards/hw_defs/ob26.json](../../boards/hw_defs/ob26.json). This file describes what the firmware does today, and what is still open.

## Status

| Area | State |
|---|---|
| Boot (bootloader + firmware), 400 MHz | Working |
| SDRAM (32 MB, FMC) | Working (self-test passes at boot) |
| LCD (480x320, bit-banged 8080) | Working on the PCB |
| Nav keys (RTN/MDL/SYS/TELE), PAGE, ENTER | Working |
| Trims, toggle switches | Working |
| Rotary encoder (direction, one count per click) | Working |
| RGB status LED | Driver written, solid green once firmware is up. Not yet confirmed on hardware |
| Sticks, pots, sliders (ADC1) | Mapped, not yet verified on hardware. Expect "throttle not stable" until the gimbals are wired |
| SD card | Unverified. The UI showed "Storage Warning: Missing or bad radio data" on an earlier bring-up |
| Battery voltage, RTC battery | Faked in `board.cpp` (reports 8.4 V and 3.0 V). The real divider scale is 0.6875 |
| Audio (I2S amplifier) | Stub, no sound |
| Backlight | Not controlled by the MCU. Functions only track state |
| USB | Device enumerates (DFU/CDC). VBUS is not sensed, `usbPlugged()` always returns 0 |
| RF module | Not implemented. V1.0 has no internal module; an external module connects to J11 (S.Port only) |
| RTC | Not implemented. There is no coin cell, and the core board's VBAT pin is not connected |

## Core board (FK743M2-IIT6)

- STM32H743IIT6, 25 MHz HSE, 32.768 kHz LSE
- W9825G6 32 MB SDRAM on FMC bank 1 at `0xC0000000`
- W25Q64 8 MB QSPI flash (not used by this target)
- microSD on SDMMC1
- USB-C (full speed), DFU via the system bootloader (hold BOOT, tap RESET)
- Onboard user LED (PH7), SWD, BOOT and RESET
- A 40-pin FPC (RGB/LTDC and touch) shares nets with PI0-PI7, PE5, PE6, PH4, PH6, PD6, PA2 and PG\*. **Keep it unpopulated.**
- The FMC data pins are used by the SDRAM, so the LCD cannot use FMC. The LCD is bit-banged over GPIO.

Power: the manufacturer recommends powering from 5 V or USB. The onboard 3.3 V regulator should not supply more than about 200 mA externally, so do not power the LCD backlight from it. Running at 480 MHz generates substantial heat; this target runs at 400 MHz (VOS1).

## Power

- **Battery:** 2 x 18650 in parallel (1S), 9900 mAh each.
- **Booster:** diymore buck-boost, 5 V output, 3 A.
- **Power path:** booster, then D2 (SS34 Schottky), then power switch SW22, then the +5V rail.
- **USB:** with USB plugged into the core board, its 5 V reaches the +5V rail. D2 blocks it from flowing back into the booster.
- **Hard switch:** turning it off during an SD write can corrupt the card. This risk is accepted.
- **Battery sense:** J24 pin 1 (raw battery), then R1 100k to PA4, with R5 and C1 (100 nF) to GND. **R5 is 220 kΩ, not the 200 kΩ originally planned**, so the scale is 220/320 = **0.6875**, and a 4.2 V battery reads about 2.89 V.

Pins used by the core board itself (do not reassign):

| Function | Pins |
|---|---|
| FMC SDRAM | PD0 PD1 PD8-PD10 PD14 PD15, PE0 PE1 PE7-PE15, PF0-PF5 PF11-PF15, PG0 PG1 PG2 PG4 PG5 PG8 PG15, PH2 PH3 PH5 |
| QSPI flash | PB2 PB6 PD11 PD12 PD13 PE2 |
| microSD (SDMMC1) | PC8-PC12, PD2 |
| USB FS | PA11 PA12 |
| SWD | PA13 PA14 |
| User LED | PH7 |

## Pin map (as built in firmware)

### Analog inputs (ADC1)

Gimbal, pot and slider headers: pin 1 (square) = +3V3, pin 2 = wiper (100 nF to GND), pin 3 = GND.

| GPIO | Header | Net | Part | ADC channel | EdgeTX input |
|---|---|---|---|---|---|
| PA0 | P3-52 | LEFT_X_AXIS | J3 | INP16 | LH (left stick X) |
| PA1 | P3-49 | LEFT_Y_AXIS | J6 | INP17 | LV (left stick Y) |
| PA2 | P3-50 | RIGHT_X_AXIS | J7 | INP14 | RH (right stick X) |
| PA3 | P3-47 | RIGHT_Y_AXIS | J8 | INP15 | RV (right stick Y) |
| PA6 | P3-46 | S1 | J26 | INP3 | P1 (pot S1) |
| PC4 | P3-44 | S2 | J27 | INP4 | P2 (pot S2) |
| PC5 | P3-42 | S3 | J28 | INP8 | EXT1 (pot S3) |
| PC0 | P3-19 | SL | J15 | INP10 | SL1 (left slider) |
| PC1 | P3-20 | SR | J16 | INP11 | SL2 (right slider) |
| PA4 | P3-45 | BATT_DIV | R1 / R5 / C1 | INP18 | VBAT (battery sense, scale 0.6875) |

The RTC battery reading uses the internal VBAT channel on ADC3. It is faked, since there is no coin cell.

### Keys (active low, internal pull-up)

| GPIO | Header | Label | Part | EdgeTX key |
|---|---|---|---|---|
| PH14 | P2-13 | MDL | SW9 (nav up) | KEY_MODEL |
| PH15 | P2-14 | RTN | SW10 (nav down) | KEY_EXIT |
| PH13 | P2-16 | SYS | SW11 (nav left) | KEY_SYS |
| PC13 | P2-45 | TELE | SW12 (nav right) | KEY_TELE |
| PH4 | P2-56 | PAGE | SW13 (nav centre) | KEY_PAGEDN |
| PI9 | P2-53 | ENTER | SW21 S2 (encoder push) | KEY_ENTER |

The colour UI uses these as shortcuts: MDL opens Model Setup (long press: Manage Models), SYS opens the quick menu (long press: Tools), TELE opens UI screen setup (long press: channel monitor). With a single PAGE button, a short press is page-down and a long press goes back. Main screens only switch with PAGE when there is more than one main screen. Locking combination: SYS + MDL.

### Rotary encoder (EC11)

| GPIO | Header | Net | Signal |
|---|---|---|---|
| PA15 | P2-32 | SCROLL_A | A |
| PB3 | P2-39 | SCROLL_B | B |
| PI9 | P2-53 | ENTER | Push switch (S2) |

The common pin (C) and the switch's other pin (S1) go to GND. The pins are read through EXTI interrupts with a TIM17 debounce, using the stock EdgeTX driver. The settings are inverted direction and granularity 4 (one count per click). `rotaryEncoderInit()` must be called from `boardInit()`, after `timersInit()`. It is not called from the bootloader.

### Trims (active low)

| EdgeTX trim | Decrement (GPIO, header, part) | Increment (GPIO, header, part) |
|---|---|---|
| T1 (left X) | PG11, P2-40, SW4 | PG14, P2-28, SW7 |
| T2 (left Y) | PG7, P2-20, SW2 | PG6, P2-17, SW1 |
| T3 (right Y) | PG13, P2-52, SW6 | PG12, P2-25, SW5 |
| T4 (right X) | PG9, P2-38, SW8 | PG10, P2-37, SW3 |

This matches the V1.0 schematic. An earlier draft of the pin map listed the left-X and right-X increment pins the other way round; that draft is obsolete.

### Toggle switches (active low, centre pin to GND)

Each switch has a 3-pin header with pin 2 (middle) at GND.

| Switch | Type | Part | UP (GPIO, header) | DN (GPIO, header) |
|---|---|---|---|---|
| SA | 2-pos | J14 | PI10, P2-54 | none |
| SB | 3-pos | J20 | PF10, P2-51 | PB14, P3-25 |
| SC | 2-pos | J17 | PF8, P3-17 | none |
| SD | 3-pos | J21 | PE3, P2-50 | PH12, P3-27 |
| SE | 2-pos | J18 | PF9, P3-18 | none |
| SF | 3-pos | J22 | PH8, P3-28 | PH9, P3-31 |
| SG | 2-pos | J19 | PE4, P2-49 | none |
| SH | 3-pos | J23 | PB11, P3-33 | PB10, P3-36 |

In `ob26.json`, `pin_low` is the UP net and `pin_high` is the DN net, because of how EdgeTX's 3-position driver decodes the pins.

### LCD — ILI9486 3.5" shield, 8-bit 8080 parallel

| GPIO | Header | Signal | Shield pin |
|---|---|---|---|
| PI0 | P2-11 | D0 | D8 |
| PI1 | P2-12 | D1 | D9 |
| PI2 | P2-27 | D2 | D2 |
| PI3 | P2-30 | D3 | D3 |
| PI4 | P3-35 | D4 | D4 |
| PI5 | P3-37 | D5 | D5 |
| PI6 | P3-38 | D6 | D6 |
| PI7 | P3-39 | D7 | D7 |
| PD3 | P2-34 | RD (held high) | A0 |
| PD7 | P2-35 | WR | A1 |
| PD6 | P2-26 | RS (D/C) | A2 |
| PD5 | P2-36 | CS | A3 |
| PD4 | P2-33 | RST | A4 |

- The data bus is the low byte of GPIOI, so one register write sets all eight bits.
- The shield is powered from +5V and +3V3 and draws up to about 120 mA with the backlight on.
- The backlight is always on. No MCU pin controls it.
- Not connected: the shield's SD-card pins, A5, AREF, VIN, RESET, D0 and D1. The core board's microSD is used instead.
- See "LCD driver" below.

### RGB status LED (common cathode, high = on)

Each colour goes through a 100 Ω resistor. D1 pin 3 (long leg) is the common cathode, to GND.

| GPIO | Header | Net | Colour | Path |
|---|---|---|---|---|
| PE5 | P2-48 | LED_R | Red | R2 to D1 pin 1 |
| PE6 | P2-47 | LED_G | Green | R3 to D1 pin 2 |
| PH6 | P3-34 | LED_B | Blue | R4 to D1 pin 4 |

These are plain GPIO outputs: one colour at a time, no dimming. The pins are also on the core board's unused FPC connector.

### External module, audio and I2C (not yet in firmware)

| GPIO | Header | Net | Part | Function |
|---|---|---|---|---|
| PA9 | P3-8 | MOD_SPORT | J11 pin 1 | USART1 half-duplex S.Port |
| PB13 | P3-23 | I2S_BCLK | J25 pin 3 | I2S2_CK |
| PB12 | P3-26 | I2S_LRC | J25 pin 4 | I2S2_WS |
| PB15 | P3-24 | I2S_DIN | J25 pin 5 | I2S2_SDO |
| PB8 | P2-43 | I2C_SCL | J30 pin 3 | I2C1 SCL |
| PB9 | P2-46 | I2C_SDA | J30 pin 4 | I2C1 SDA |

- **J11 (external module):** pin 1 = S.Port, pin 2 = GND, pin 3 = +5V. An external module connects with 3 wires, for S.Port only. PA9 is also USART1 TX (AUX1 debug) in `hal.h`, so the two clash.
- **J25 (I2S audio):** pin 1 = +5V, pin 2 = GND. It drives a MAX98357A amplifier and 3 W speaker kit, with GAIN and SD left at the kit's defaults.
- **J30 (I2C):** pin 1 = +3V3, pin 2 = GND. It is for a future RTC breakout and is unused for now. The PCB has no I2C pull-ups.

## Free and unused pins

- **Free GPIOs:** PA5, PA7, PA8, PA10, PB0, PB1, PB4, PB5, PC2_C, PC3_C, PF6, PF7, PG3, PH10, PH11, PI8, PI11.
- **Shared pins:** none.
- **Core board's 40-pin FPC:** not used.
- **Not on the board:** buzzer, haptic motor, test points, UART header, SWD header.

## Firmware layout

- Bootloader (128 KB) at `0x08000000`, firmware at `0x08020000`, both in internal flash. `firmware.bin` contains both.
- Timers: TIM12 mixer scheduler, TIM14 millisecond timer, TIM17 encoder debounce.
- DMA2: stream 0 ADC3, stream 4 ADC1, streams 1 and 2 AUX1 serial.
- `hal.h` defines the AUX1 serial port (USART1 on PA9/PA10). It is used only when built with `DEBUG`.
- Settings storage uses a generated `yaml_datastructs_ob26.cpp`.
- `per5ms()` is an empty stub. EdgeTX calls it every 5 ms, and the firmware needs it to link.
- Build flags specific to this target: `PCBOB26` (set in `CMakeLists.txt`) and `RADIO_OB26` (derived by the build from the flavour name).

### LCD driver

[lcd_driver.cpp](lcd_driver.cpp) drives the panel as an 8080 bus over GPIO.

- Landscape 480x320, RGB565 (two bus writes per pixel).
- One `BSRR` store puts a byte on D0-D7, then WR is pulsed low and high. WR phases are timed on the DWT cycle counter (500 ns each by default, `LCD_WR_NS`). Lower values may work and have not been tuned.
- GPIO edges are set to the lowest speed. Fast edges ring on long wires and double-clock WR.
- Init sequence: `0x01` reset, `0x11` sleep out, `0x3A=0x55`, power control `C0/C1/C2`, VCOM `C5`, display function `B6`, `0x36=0x28` (MADCTL), `0x20` inversion off, `0x29` display on.
- EdgeTX hands over the whole frame every flush, so the driver keeps a shadow copy in SDRAM, diffs it row by row and sends only the changed rectangles (`0x2A`, `0x2B`, `0x2C`).

## Build

```
export PATH=/home/ob/dev/edgetx/.venv/bin:$HOME/.local/toolchains/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/bin:$PATH
cmake --preset ob26
cmake --build build/ob26 --target firmware -j20
```

The `.venv` is required (the hardware-definition generator needs pydantic and lz4). Output: `build/ob26/firmware.bin`.

## Flash

- **WSL:** put the board in DFU (hold BOOT, tap RESET). In an admin PowerShell run `usbipd attach --wsl --busid 1-6`, then `sudo dfu-util -a 0 -s 0x08000000:leave -D build/ob26/firmware.bin`. The trailing "Error during download get_status" is normal. Detach with `usbipd detach --busid 1-6` before using Windows.
- **Windows:** STM32CubeProgrammer, USB mode, program the `.bin` at `0x08000000`. The GUI does not reset the board afterwards, so press RESET. Until then a solid white screen is normal.
- Debugging: SWD with an ST-LINK, J-Link or CMSIS-DAP probe. The core board has no USB-UART converter.

## Menus for testing hardware

- **SYS → Radio Settings → Hardware:** calibration, a live stick view, and Debug → Keys and Analogs for raw inputs.
- Never flash the TX15 or any other target's firmware on this board.

## Open items

Firmware:

- Verify sticks, pots and sliders; calibrate after wiring.
- Debug the SD card "Storage Warning".
- Battery voltage scaling on PA4 (scale 0.6875, so 4.2 V reads about 2.89 V), replacing the faked value.
- J11 S.Port (half-duplex) definition for an external module.
- I2S2 audio driver for the MAX98357A.
- RTC: there is no coin cell and VBAT is not connected, so the clock is lost at power-off. A future I2C RTC breakout fits J30, but EdgeTX has no external I2C RTC driver that I know of.
- Optional: use EdgeTX's native status-LED feature for colour per state and brightness. This changes the saved settings layout.
- Optional: tune `LCD_WR_NS`.
- Decide where the AUX1 debug serial goes. PA9 is used by J11, and PA10 is free but is only the RX half of USART1.

PCB / hardware:

- Add I2C pull-ups if an RTC breakout is used on J30.
- Optional debounce capacitors on the encoder A/B lines.
- Keep the core board's 40-pin FPC unpopulated.
- Accepted risk: turning the hard power switch off during an SD write can corrupt the card.

## Reference documents

1. FK743M2-IIT6 electrical schematic and mechanical dimensions (V1.0)
2. ST AN4938 Rev 5, STM32H7 hardware design
3. ST DS12110 Rev 7, STM32H743 datasheet
4. ST ES0392 Rev 8, STM32H743 errata
5. ST PM0253 Rev 5, Cortex-M7 programming manual
6. FANK E: USB download tutorial V1.1, power and consumption guidance, external flash programming guide V1.0
