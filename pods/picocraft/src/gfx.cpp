// PicoCraft - 2D primitives for the HUD and menus on the low-resolution frame.
#include "gfx.h"
#include "config.h"
#include "textures.h"

#define G(a, b, c, d, e) (uint16_t)((0##a << 12) | (0##b << 9) | (0##c << 6) | (0##d << 3) | 0##e)
// 3-bit rows written as octal "binary": 7=111, 5=101, 2=010, 4=100, 1=001, 6=110, 3=011
static const struct { char ch; uint16_t g; } font[] = {
    {'0', G(7,5,5,5,7)}, {'1', G(2,6,2,2,7)}, {'2', G(7,1,7,4,7)}, {'3', G(7,1,7,1,7)},
    {'4', G(5,5,7,1,1)}, {'5', G(7,4,7,1,7)}, {'6', G(7,4,7,5,7)}, {'7', G(7,1,1,2,2)},
    {'8', G(7,5,7,5,7)}, {'9', G(7,5,7,1,7)},
    {'A', G(2,5,7,5,5)}, {'B', G(6,5,6,5,6)}, {'C', G(3,4,4,4,3)}, {'D', G(6,5,5,5,6)},
    {'E', G(7,4,6,4,7)}, {'F', G(7,4,6,4,4)}, {'G', G(3,4,5,5,3)}, {'H', G(5,5,7,5,5)},
    {'I', G(7,2,2,2,7)}, {'J', G(1,1,1,5,2)}, {'K', G(5,5,6,5,5)}, {'L', G(4,4,4,4,7)},
    {'M', G(5,7,7,5,5)}, {'N', G(6,5,5,5,5)}, {'O', G(2,5,5,5,2)}, {'P', G(6,5,6,4,4)},
    {'Q', G(2,5,5,6,3)}, {'R', G(6,5,6,5,5)}, {'S', G(3,4,2,1,6)}, {'T', G(7,2,2,2,2)},
    {'U', G(5,5,5,5,7)}, {'V', G(5,5,5,5,2)}, {'W', G(5,5,7,7,5)}, {'X', G(5,5,2,5,5)},
    {'Y', G(5,5,2,2,2)}, {'Z', G(7,1,2,4,7)},
    {'.', G(0,0,0,0,2)}, {',', G(0,0,0,2,4)}, {':', G(0,2,0,2,0)}, {'!', G(2,2,2,0,2)},
    {'?', G(6,1,2,0,2)}, {'-', G(0,0,7,0,0)}, {'+', G(0,2,7,2,0)}, {'/', G(1,1,2,4,4)},
    {'(', G(1,2,2,2,1)}, {')', G(4,2,2,2,4)}, {'\'', G(2,2,0,0,0)}, {'>', G(4,2,1,2,4)},
    {'<', G(1,2,4,2,1)}, {'=', G(0,7,0,7,0)}, {'%', G(5,1,2,4,5)}, {'_', G(0,0,0,0,7)},
    {'[', G(3,2,2,2,3)}, {']', G(6,2,2,2,6)}, {'*', G(0,5,2,5,0)}, {'#', G(5,7,5,7,5)},
    {'^', G(2,5,0,0,0)},
};

static uint16_t glyph(char ch) {
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    for (unsigned i = 0; i < sizeof font / sizeof font[0]; i++)
        if (font[i].ch == ch) return font[i].g;
    return 0;
}

static inline void put(uint16_t *fb, int x, int y, uint16_t c) {
    if ((unsigned)x < RW && (unsigned)y < RH) fb[y * RW + x] = c;
}

void gfx_fill(uint16_t *fb, int x, int y, int w, int h, uint16_t c) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) put(fb, i, j, c);
}

void gfx_frame(uint16_t *fb, int x, int y, int w, int h, uint16_t c) {
    gfx_fill(fb, x, y, w, 1, c);
    gfx_fill(fb, x, y + h - 1, w, 1, c);
    gfx_fill(fb, x, y, 1, h, c);
    gfx_fill(fb, x + w - 1, y, 1, h, c);
}

void gfx_darken(uint16_t *fb, int x, int y, int w, int h) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if ((unsigned)i < RW && (unsigned)j < RH) {
                uint16_t *p = &fb[j * RW + i];
                *p = (*p >> 2) & 0x39E7;
            }
}

static void text_raw(uint16_t *fb, int x, int y, const char *s, uint16_t c, int sc) {
    for (; *s; s++, x += 4 * sc) {
        uint16_t g = glyph(*s);
        for (int r = 0; r < 5; r++)
            for (int b = 0; b < 3; b++)
                if (g & (1 << ((4 - r) * 3 + (2 - b))))
                    gfx_fill(fb, x + b * sc, y + r * sc, sc, sc, c);
    }
}

void gfx_text(uint16_t *fb, int x, int y, const char *s, uint16_t c, int scale, int shadow) {
    if (shadow) text_raw(fb, x + 1, y + 1, s, C_BLACK, scale);
    text_raw(fb, x, y, s, c, scale);
}

int gfx_text_w(const char *s, int scale) {
    int n = 0;
    while (s[n]) n++;
    return n ? (n * 4 - 1) * scale : 0;
}

void gfx_text_c(uint16_t *fb, int cx, int y, const char *s, uint16_t c, int scale, int shadow) {
    gfx_text(fb, cx - gfx_text_w(s, scale) / 2, y, s, c, scale, shadow);
}

void gfx_icon(uint16_t *fb, int x, int y, int block, int size) {
    for (int j = 0; j < size; j++)
        for (int i = 0; i < size; i++) {
            uint16_t c = tex[block][1][(j * 16 / size) * 16 + (i * 16 / size)];
            if (c == TEX_CLEAR) continue;
            put(fb, x + i, y + j, c);
        }
}

void gfx_crosshair(uint16_t *fb) {
    int cx = RW / 2, cy = RH / 2;
    for (int i = -3; i <= 3; i++) {
        if (i == 0) continue;
        uint16_t *p = &fb[cy * RW + cx + i]; *p = ~*p;
        p = &fb[(cy + i) * RW + cx]; *p = ~*p;
    }
}
