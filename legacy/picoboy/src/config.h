// PicoBoy - hardware configuration for the ClockworkPi PicoCalc + Raspberry Pi Pico 2 W
// Pin numbers come from ClockworkPi's reference code and the UF2 Loader sources.
#pragma once

// ---------------------------------------------------------------- clocks
#define SYS_CLOCK_KHZ        200000   // RP2350 overclock; 150000 is stock. Drop if unstable.

// ---------------------------------------------------------------- LCD
// Pins, SPI speed (25 MHz) and panel setup live in lib/picocalc_ui (pcui::lcd::Config).

// ---------------------------------------------------------------- keyboard (STM32 south bridge, I2C1)
#define KBD_I2C              i2c1
#define KBD_PIN_SDA          6
#define KBD_PIN_SCL          7
#define KBD_I2C_HZ           10000    // ClockworkPi recommends 10 kHz
#define KBD_ADDR             0x1F
#define KBD_READ_GAP_US      16000    // gap between register write and read (reference code uses 16 ms;
                                      // try lower values for snappier input)

// ---------------------------------------------------------------- SD card (SPI0)
#define SD_SPI               spi0
#define SD_PIN_SCK           18
#define SD_PIN_MOSI          19
#define SD_PIN_MISO          16
#define SD_PIN_CS            17
#define SD_PIN_DETECT        22
#define SD_SPI_HZ            25000000

// ---------------------------------------------------------------- audio (PWM)
// Mainboard v2.0 routes left to GP28 (older reference code used GP26).
#define AUDIO_PIN_L          28
#define AUDIO_PIN_R          27

// ---------------------------------------------------------------- emulator behaviour
#define ROM_DIR              "/roms"      // browser starts here if it exists, else "/"
#define ROMCACHE_MAX_BYTES   (2u * 1024 * 1024 + 4096)   // flash reserved for the running ROM
#define CART_RAM_MAX         (32 * 1024)     // largest DMG save RAM; 128 KB carts are GBC titles
#define AUTOSAVE_FRAMES      180          // save .sav ~3 s after the game last wrote to cart RAM
#define DEFAULT_VOLUME       60           // percent

// Key bindings (PicoCalc keyboard codes; letters are matched case-insensitively)
#define KEY_UP               0xB5
#define KEY_DOWN             0xB6
#define KEY_LEFT             0xB4
#define KEY_RIGHT            0xB7
#define KEY_ENTER            0x0A
#define KEY_ESC              0xB1
#define KEY_BACKSPACE        0x08
#define KEY_F1               0x81

#define BIND_A1              'x'
#define BIND_A2              'k'
#define BIND_B1              'z'
#define BIND_B2              'j'
#define BIND_START           KEY_ENTER
#define BIND_SELECT1         ' '
#define BIND_SELECT2         KEY_BACKSPACE
#define BIND_PAUSE1          KEY_ESC
#define BIND_PAUSE2          KEY_F1
