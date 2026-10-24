// ClockworkPi PicoCalc pin map and tunables (Pico 2 W module).
#pragma once

// ---- Display: 320x320 IPS, ST7365P (ILI9488-compatible) on SPI1 ----------
#define LCD_SPI        spi1
#define LCD_SCK_PIN    10
#define LCD_MOSI_PIN   11
#define LCD_MISO_PIN   12
#define LCD_CS_PIN     13
#define LCD_DC_PIN     14
#define LCD_RST_PIN    15
#define LCD_SPI_HZ     (25 * 1000 * 1000)  // same as the ClockworkPi driver
#define LCD_WIDTH      320
#define LCD_HEIGHT     320
// If colours look inverted/negative on your unit, flip this to 0.
#define LCD_INVERT     1
// Memory access control. 0x48 = column-mirror + BGR (PicoCalc default).
#define LCD_MADCTL     0x48

// ---- Keyboard / south bridge: STM32 on I2C1 ---------------------------------
#define KBD_I2C        i2c1
#define KBD_SDA_PIN    6
#define KBD_SCL_PIN    7
#define KBD_I2C_HZ     (10 * 1000)  // the STM32 firmware expects a slow bus
// Delay between writing the register and reading the reply. 16 ms matches
// the ClockworkPi examples and the UF2 Loader; shorter delays can confuse
// some keyboard (STM32 BIOS) firmware versions.
#define KBD_READ_DELAY_US 16000

// ---- SD card slot on SPI0 ---------------------------------------------------
#define SD_SPI         spi0
#define SD_SCK_PIN     18
#define SD_MOSI_PIN    19
#define SD_MISO_PIN    16
#define SD_CS_PIN      17
#define SD_DETECT_PIN  22
#define SD_SPI_HZ      (25 * 1000 * 1000)
