// PicoCraft - block textures (RGB565).
#pragma once
#include <cstdint>

#include "world.h"

#define TEX_CLEAR 0x0001   // transparent texel (leaves, glass)

// Shading variants: 0 top, 1 X side, 2 Z side, 3 bottom
extern uint16_t tex[B_COUNT][4][256];

void textures_init(void);

static inline uint16_t rgb565(int r, int g, int b) {
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
