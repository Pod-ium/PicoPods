// picocalc_ui - text rendering (adapted from PicoCalc Writer's gfx.cpp)
#include "pcui/gfx.h"
#include "fonts_terminus.h"

namespace pcui {
namespace gfx {
namespace {

uint8_t g_buf[SCREEN_W * 24 * lcd::BPP];

inline int glyph_index(uint32_t cp) {
    if (cp >= 0x20 && cp < 0x7F) return int(cp - 0x20);
    if (cp >= 0xA0 && cp <= 0xFF) return int(cp - 0xA0 + 95);
    return '?' - 0x20;
}

uint32_t decode_utf8(const uint8_t* s, int* used) {
    const uint8_t c = s[0];
    int n = 1;
    uint32_t cp = c;
    if (c >= 0xC0 && c < 0xE0) { n = 2; cp = c & 0x1F; }
    else if (c >= 0xE0 && c < 0xF0) { n = 3; cp = c & 0x0F; }
    else if (c >= 0xF0 && c < 0xF8) { n = 4; cp = c & 0x07; }
    else if (c >= 0x80) { *used = 1; return '?'; }
    for (int i = 1; i < n; ++i) {
        if ((s[i] & 0xC0) != 0x80) { *used = i; return '?'; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *used = n;
    return cp;
}

}  // namespace

const Font& font(int id) {
    if (id < 0 || id > 3) id = 1;
    return *kFonts[id];
}

int text_cells(const char* s) {
    int n = 0;
    for (; *s; ++s)
        if ((uint8_t(*s) & 0xC0) != 0x80) ++n;
    return n;
}

int text(int x, int y, int width_px, const Font& f, const char* s, Color fg, Color bg) {
    if (width_px <= 0) width_px = text_cells(s) * f.w;
    if (x + width_px > SCREEN_W) width_px = SCREEN_W - x;
    if (width_px <= 0 || y < 0 || y + f.h > SCREEN_H) return 0;
    const int h = f.h;
    uint8_t fp[3], bp[3];
    lcd::encode(fg, fp);
    lcd::encode(bg, bp);
    const int total = width_px * h;
    for (int i = 0; i < total; ++i) {
        g_buf[3 * i] = bp[0];
        g_buf[3 * i + 1] = bp[1];
        g_buf[3 * i + 2] = bp[2];
    }
    const int gsize = h * f.bpr;
    int cells = 0;
    const uint8_t* p = (const uint8_t*)s;
    while (*p) {
        const int px0 = cells * f.w;
        if (px0 + f.w > width_px) break;
        int used = 1;
        const uint32_t cp = decode_utf8(p, &used);
        p += used;
        const uint8_t* gl = f.data + glyph_index(cp) * gsize;
        for (int yy = 0; yy < h; ++yy) {
            const uint8_t* row = gl + yy * f.bpr;
            uint8_t* out = g_buf + 3 * (yy * width_px + px0);
            for (int xx = 0; xx < f.w; ++xx) {
                if (row[xx >> 3] & (0x80 >> (xx & 7))) {
                    out[3 * xx] = fp[0];
                    out[3 * xx + 1] = fp[1];
                    out[3 * xx + 2] = fp[2];
                }
            }
        }
        ++cells;
    }
    lcd::set_window(x, y, x + width_px - 1, y + h - 1);
    lcd::write(g_buf, size_t(total) * 3);
    lcd::end();
    return cells;
}

void fill(int x, int y, int w, int h, Color c) { lcd::fill(x, y, w, h, c); }

void frame(int x, int y, int w, int h, Color c) {
    lcd::fill(x, y, w, 1, c);
    lcd::fill(x, y + h - 1, w, 1, c);
    lcd::fill(x, y, 1, h, c);
    lcd::fill(x + w - 1, y, 1, h, c);
}

}  // namespace gfx
}  // namespace pcui
