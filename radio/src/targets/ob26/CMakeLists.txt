option(UNEXPECTED_SHUTDOWN "Enable the Unexpected Shutdown screen" ON)
option(PXX1 "PXX1 protocol support" OFF)
option(PXX2 "PXX2 protocol support" OFF)
option(AFHDS3 "AFHDS3 TX Module" OFF)
option(MULTIMODULE "DIY Multiprotocol TX Module (https://github.com/pascallanger/DIY-Multiprotocol-TX-Module)" ON)
option(GHOST "Ghost TX Module" OFF)
option(MODULE_SIZE_STD "Standard size TX Module" ON)
option(LUA_MIXER "Enable LUA mixer/model scripts support" ON)
option(DISK_CACHE "Enable SD card disk cache" ON)

# OB26: custom radio built around the FANK FK743M2-IIT6 core board
#   STM32H743IIT6, 25 MHz HSE, 32 MB SDRAM (W9825G6 on FMC bank 1),
#   W25Q64 QSPI flash, microSD (SDMMC1), USB-C (OTG FS)
#
# The H743 has 2 MB of internal flash, so the bootloader and the firmware
# both live in internal flash (see linker/stm32h743_sdram).

set(CPU_TYPE STM32H7)
set(HSE_VALUE 25000000)
set(SDCARD YES)
set(STORAGE_MODELSLIST YES)
set(GUI_DIR colorlcd)
set(BITMAPS_DIR 480x272)
set(TARGET_DIR ob26)
set(PPM_LIMITS_SYMETRICAL YES)
set(USB_SERIAL ON CACHE BOOL "Enable USB serial (CDC)")

# for size report script
set(CPU_TYPE_FULL STM32H743xx)
set(TARGET_LINKER_DIR stm32h743_sdram)
set(TARGET_EXTRAM_START 0xC0000000)
set(TARGET_EXTRAM_SIZE 32M)
set(SIZE_TARGET_MEM_DEFINE "MEM_SIZE_SDRAM1=32768")

add_definitions(-DPCBOB26 -DPCBHORUS)
add_definitions(-DSTM32_SUPPORT_32BIT_TIMERS)

set(FLAVOUR ob26)

set(BITMAPS_TARGET bm480_bitmaps)
set(FONTS_TARGET x12_fonts)
set(LCD_DRIVER lcd_driver.cpp)
set(RADIO_DEPENDENCIES ${RADIO_DEPENDENCIES} ${BITMAPS_TARGET})
set(FIRMWARE_DEPENDENCIES datacopy)

add_definitions(
  -DSTM32H743xx
  -DSDRAM -DCCMRAM
  -DHARDWARE_KEYS
  -DSOFTWARE_VOLUME
)

set(SDRAM ON)
set(AUX_SERIAL YES)

add_definitions(-DAUDIO -DVOICE)

# RGB status LED: colour per state and brightness (EdgeTX's stock LED driver)
add_definitions(-DSTATUS_LED_PWM)

if(NOT UNEXPECTED_SHUTDOWN)
  add_definitions(-DNO_UNEXPECTED_SHUTDOWN)
endif()

if(DISK_CACHE)
  set(SRC ${SRC} disk_cache.cpp)
  add_definitions(-DDISK_CACHE)
endif()

# VCP CLI
set(ENABLE_SERIAL_PASSTHROUGH ON CACHE BOOL "Enable serial passthrough")
set(CLI ON CACHE BOOL "Enable CLI")

set(TARGET_SRC_DIR targets/${TARGET_DIR})

set(BOARD_COMMON_SRC
  ${TARGET_SRC_DIR}/board.cpp
  ${TARGET_SRC_DIR}/key_driver.cpp
  ${TARGET_SRC_DIR}/lcd_driver.cpp
  ${TARGET_SRC_DIR}/backlight_driver.cpp
  ${TARGET_SRC_DIR}/sdram_driver.cpp
  targets/common/arm/stm32/abnormal_reboot.cpp
  targets/common/arm/stm32/delays_driver.cpp
  targets/common/arm/stm32/dma2d.cpp
  targets/common/arm/stm32/flash_driver.cpp
  targets/common/arm/stm32/rotary_encoder_driver.cpp
  targets/common/arm/stm32/pwr_driver.cpp
  targets/common/arm/stm32/rtc_driver.cpp
  targets/common/arm/stm32/watchdog_driver.cpp
)

# Bootloader board library
add_library(board_bl OBJECT EXCLUDE_FROM_ALL
  ${BOARD_COMMON_SRC}
  ${RADIO_SRC_DIR}/gui/colorlcd/boot_menu.cpp
)
add_dependencies(board_bl minimal_board_lib)
set(BOOTLOADER_SRC ${BOOTLOADER_SRC} $<TARGET_OBJECTS:board_bl>)

set(CMSIS_SRC ${TARGET_SRC_DIR}/system_clock.c)

# Firmware board library
add_library(board OBJECT EXCLUDE_FROM_ALL
  ${BOARD_COMMON_SRC}
  ${TARGET_SRC_DIR}/audio_driver.cpp
  targets/common/arm/stm32/heartbeat_driver.cpp
  targets/common/arm/stm32/mixer_scheduler_driver.cpp
  targets/common/arm/stm32/stm32_switch_driver.cpp
)
add_dependencies(board board_lib)
set(FIRMWARE_SRC ${FIRMWARE_SRC} $<TARGET_OBJECTS:board>)

include_directories(${RADIO_SRC_DIR}/fonts/colorlcd gui/${GUI_DIR} gui/${GUI_DIR}/layouts)

if(BOOTLOADER)
  set(FIRMWARE_TARGET_SRC
    ${FIRMWARE_TARGET_SRC}
    ../../bootloader/loadboot.cpp
  )
endif()

# Make malloc() thread-safe
add_definitions(-DTHREADSAFE_MALLOC)

if (MULTIMODULE)
  add_definitions(-DMULTI_PROTOLIST)
  set(SRC ${SRC}
    io/multi_protolist.cpp
    )
endif()
