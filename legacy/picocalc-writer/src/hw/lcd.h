// Writer's LCD API (RGB565 colours). Since 2.1 it is a thin layer over the
// shared picocalc_ui driver (lib/picocalc_ui/include/pcui/lcd.h).
#pragma once
#include <cstddef>
#include <cstdint>

namespace lcd {
void init();
void setWindow(int x0, int y0, int x1, int y1);
// Stream pixel data into the current window: 3 bytes per pixel (R, G, B),
// the panel's 18-bit serial format. Use expand() to convert RGB565.
void writePixels(const uint8_t* data, size_t bytes);
void endPixels();
void fillRect(int x, int y, int w, int h, uint16_t color);

constexpr int kBytesPerPixel = 3;

// RGB565 -> the 3 bytes the panel expects (8-bit channels, top 6 bits used).
inline void expand(uint16_t c, uint8_t out[3]) {
    const uint8_t r5 = (c >> 11) & 0x1F, g6 = (c >> 5) & 0x3F, b5 = c & 0x1F;
    out[0] = uint8_t((r5 << 3) | (r5 >> 2));
    out[1] = uint8_t((g6 << 2) | (g6 >> 4));
    out[2] = uint8_t((b5 << 3) | (b5 >> 2));
}

// RGB565 -> 0xRRGGBB, the colour type of picocalc_ui. Same bit replication
// as expand(), so a colour looks identical whichever API draws it.
constexpr uint32_t to888(uint16_t c) {
    return (uint32_t((((c >> 11) & 0x1F) << 3) | (((c >> 11) & 0x1F) >> 2)) << 16) |
           (uint32_t((((c >> 5) & 0x3F) << 2) | (((c >> 5) & 0x3F) >> 4)) << 8) |
           uint32_t(((c & 0x1F) << 3) | ((c & 0x1F) >> 2));
}

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
}  // namespace lcd
