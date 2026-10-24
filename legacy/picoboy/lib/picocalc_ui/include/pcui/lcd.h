// picocalc_ui - LCD driver for the ClockworkPi PicoCalc 320x320 panel (ST7365P).
// Based on the PicoCalc Writer driver. The panel only accepts 18-bit colour over
// SPI: every pixel is 3 bytes (R, G, B, top 6 bits used).
#pragma once
#include <cstddef>
#include <cstdint>
#include "hardware/spi.h"

namespace pcui {

using Color = uint32_t;   // 0xRRGGBB
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 320;

namespace lcd {

constexpr int BPP = 3;

struct Config {
    spi_inst_t* spi = spi1;
    uint sck = 10, mosi = 11, miso = 12, cs = 13, dc = 14, rst = 15;
    uint32_t hz = 25000000;     // same as ClockworkPi's driver
    uint8_t madctl = 0x48;      // column mirror + BGR
    bool invert = true;         // the PicoCalc panel needs INVON
};

void init(const Config& cfg = Config());

// Opens a pixel write into the given (inclusive) window. Chip select stays low
// until end(): the window setup and the pixels must share one transaction.
void set_window(int x0, int y0, int x1, int y1);
void write(const uint8_t* data, size_t bytes);
void write_dma(const uint8_t* data, size_t bytes);   // asynchronous
bool dma_busy();
void wait_dma();
void end();
int dma_channel();

void fill(int x, int y, int w, int h, Color c);

inline void encode(Color c, uint8_t out[3]) {
    out[0] = uint8_t(c >> 16);
    out[1] = uint8_t(c >> 8);
    out[2] = uint8_t(c);
}

}  // namespace lcd
}  // namespace pcui
