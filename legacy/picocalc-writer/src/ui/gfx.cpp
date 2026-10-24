#include "gfx.h"

#include "../hw/lcd.h"

namespace gfx {
namespace {

using lcd::rgb;

const Theme kThemes[kThemeCount] = {
    // name     bg              fg                dim               sel
    {"Night", rgb(18, 18, 20), rgb(214, 208, 196), rgb(110, 108, 104), rgb(52, 70, 92),
     // cursor         statusBg          statusFg
     rgb(232, 160, 64), rgb(26, 26, 30), rgb(120, 118, 112),
     // panelBg        panelFg            panelHi           panelHiFg         accent
     rgb(34, 34, 40), rgb(214, 208, 196), rgb(232, 160, 64), rgb(18, 18, 20), rgb(232, 160, 64),
     // syntax: plain, keyword, type, string, number, comment, meta,
     //         tag, attr, punct, heading, emph, code, link
     {0, rgb(232, 160, 64), rgb(126, 176, 212), rgb(150, 196, 120), rgb(214, 150, 110),
      rgb(112, 110, 106), rgb(192, 138, 200), rgb(126, 176, 212), rgb(220, 190, 120),
      rgb(128, 124, 116), rgb(240, 176, 80), rgb(236, 186, 150), rgb(150, 196, 120),
      rgb(126, 176, 212)}},
    {"Paper", rgb(244, 239, 228), rgb(43, 43, 43), rgb(140, 132, 116), rgb(200, 214, 230),
     rgb(192, 80, 16), rgb(234, 228, 214), rgb(130, 122, 106),
     rgb(252, 249, 242), rgb(43, 43, 43), rgb(192, 80, 16), rgb(252, 249, 242), rgb(192, 80, 16),
     {0, rgb(176, 64, 16), rgb(32, 96, 150), rgb(56, 120, 40), rgb(160, 90, 20),
      rgb(146, 138, 122), rgb(128, 64, 144), rgb(32, 96, 150), rgb(150, 100, 20),
      rgb(140, 132, 116), rgb(192, 80, 16), rgb(130, 70, 40), rgb(56, 120, 40),
      rgb(32, 96, 150)}},
    {"Phosphor", rgb(0, 10, 2), rgb(80, 255, 120), rgb(30, 120, 50), rgb(20, 70, 30),
     rgb(180, 255, 190), rgb(0, 18, 6), rgb(30, 130, 55),
     rgb(0, 24, 8), rgb(80, 255, 120), rgb(80, 255, 120), rgb(0, 10, 2), rgb(80, 255, 120),
     // monochrome: brightness levels only
     {0, rgb(200, 255, 210), rgb(150, 255, 170), rgb(50, 200, 90), rgb(130, 240, 150),
      rgb(30, 125, 55), rgb(110, 225, 140), rgb(200, 255, 210), rgb(110, 215, 130),
      rgb(45, 165, 80), rgb(210, 255, 220), rgb(170, 255, 190), rgb(50, 200, 90),
      rgb(140, 245, 170)}},
};

// One pixel row. Bands are streamed row by row, so Writer needs 960 bytes
// here instead of a 23 KB band buffer (picocalc_ui keeps its own for menus).
uint8_t g_row[LCD_W_MAX * 3];
constexpr int kMaxCells = LCD_W_MAX / 6 + 1;  // narrowest font is 6 px

inline int glyphIndex(uint32_t cp) {
    if (cp >= 0x20 && cp < 0x7F) return int(cp - 0x20);
    if (cp >= 0xA0 && cp <= 0xFF) return int(cp - 0xA0 + 95);
    if (cp == '\t') return 0;
    return '?' - 0x20;
}

}  // namespace

const Theme& theme(int idx) { return kThemes[(idx % kThemeCount + kThemeCount) % kThemeCount]; }

uint32_t decodeUtf8(const uint8_t* s, uint32_t len, int* used) {
    const uint8_t c = s[0];
    int n = 1;
    uint32_t cp = c;
    if (c >= 0xC0 && c < 0xE0) { n = 2; cp = c & 0x1F; }
    else if (c >= 0xE0 && c < 0xF0) { n = 3; cp = c & 0x0F; }
    else if (c >= 0xF0 && c < 0xF8) { n = 4; cp = c & 0x07; }
    else if (c >= 0x80) { *used = 1; return '?'; }
    if (uint32_t(n) > len) { *used = 1; return '?'; }
    for (int i = 1; i < n; ++i) {
        if ((s[i] & 0xC0) != 0x80) { *used = 1; return '?'; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *used = n;
    return cp;
}

int textCells(const std::string& s) {
    int n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;
    return n;
}

void drawCells(int x, int y, int widthPx, const Font& f, const Cell* cells, int n, uint16_t bg,
               int cursorCol, uint16_t cursorColor) {
    if (widthPx <= 0) return;
    if (widthPx > LCD_W_MAX) widthPx = LCD_W_MAX;
    const int h = f.h;
    if (n > widthPx / f.w) n = widthPx / f.w;  // only whole cells are drawn
    if (n > kMaxCells) n = kMaxCells;
    // Colours are converted once per cell, not once per pixel row.
    uint8_t bp[3], cp[3];
    static uint8_t fgc[kMaxCells][3], bgc[kMaxCells][3];
    lcd::expand(bg, bp);
    lcd::expand(cursorColor, cp);
    for (int k = 0; k < n; ++k) {
        lcd::expand(cells[k].fg, fgc[k]);
        lcd::expand(cells[k].bg, bgc[k]);
    }
    int cx = -1;
    if (cursorCol >= 0) {
        cx = cursorCol * f.w;
        if (cx > widthPx - 2) cx = widthPx - 2;
    }
    lcd::setWindow(x, y, x + widthPx - 1, y + h - 1);
    for (int yy = 0; yy < h; ++yy) {
        for (int i = 0; i < widthPx; ++i) {
            g_row[3 * i] = bp[0];
            g_row[3 * i + 1] = bp[1];
            g_row[3 * i + 2] = bp[2];
        }
        for (int k = 0; k < n; ++k) {
            const uint8_t* glyphRow = f.data + glyphIndex(cells[k].cp) * h * f.bpr + yy * f.bpr;
            uint8_t* out = g_row + 3 * k * f.w;
            for (int xx = 0; xx < f.w; ++xx) {
                const uint8_t* src = (glyphRow[xx >> 3] & (0x80 >> (xx & 7))) ? fgc[k] : bgc[k];
                out[3 * xx] = src[0];
                out[3 * xx + 1] = src[1];
                out[3 * xx + 2] = src[2];
            }
        }
        if (cx >= 0) {
            for (int xx = cx; xx < cx + 2; ++xx) {
                g_row[3 * xx] = cp[0];
                g_row[3 * xx + 1] = cp[1];
                g_row[3 * xx + 2] = cp[2];
            }
        }
        lcd::writePixels(g_row, size_t(widthPx) * 3);
    }
    lcd::endPixels();
}

int drawText(int x, int y, int widthPx, const Font& f, const std::string& s, uint16_t fg,
             uint16_t bg, int cursorCol, uint16_t cursorColor) {
    static Cell cells[LCD_W_MAX / 6 + 1];
    const int maxCells = widthPx / f.w;
    int n = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(s.data());
    uint32_t left = s.size();
    while (left > 0 && n < maxCells) {
        int used = 1;
        const uint32_t cp = decodeUtf8(p, left, &used);
        cells[n++] = Cell{uint16_t(cp > 0xFFFF ? '?' : cp), fg, bg};
        p += used;
        left -= uint32_t(used);
    }
    drawCells(x, y, widthPx, f, cells, n, bg, cursorCol, cursorColor);
    return n;
}

void fill(int x, int y, int w, int h, uint16_t color) { lcd::fillRect(x, y, w, h, color); }

void frame(int x, int y, int w, int h, uint16_t color) {
    lcd::fillRect(x, y, w, 1, color);
    lcd::fillRect(x, y + h - 1, w, 1, color);
    lcd::fillRect(x, y, 1, h, color);
    lcd::fillRect(x + w - 1, y, 1, h, color);
}

}  // namespace gfx
