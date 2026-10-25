// PicoCraft - 2D drawing on the RW x RH frame (HUD, menus) with a 3x5 font.
#pragma once
#include <cstdint>

#define C_WHITE  0xFFFF
#define C_BLACK  0x0000
#define C_YELLOW 0xFFE0
#define C_GRAY   0x8410
#define C_RED    0xF800

void gfx_fill(uint16_t *fb, int x, int y, int w, int h, uint16_t c);
void gfx_frame(uint16_t *fb, int x, int y, int w, int h, uint16_t c);
void gfx_darken(uint16_t *fb, int x, int y, int w, int h);
// 3x5 text (4 px * scale per character). Lower case is drawn as upper case.
void gfx_text(uint16_t *fb, int x, int y, const char *s, uint16_t c, int scale, int shadow);
int  gfx_text_w(const char *s, int scale);
void gfx_text_c(uint16_t *fb, int cx, int y, const char *s, uint16_t c, int scale, int shadow);
void gfx_icon(uint16_t *fb, int x, int y, int block, int size);
void gfx_crosshair(uint16_t *fb);
