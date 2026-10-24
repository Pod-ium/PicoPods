// Text rendering on the LCD through a single row buffer (no framebuffer).
#pragma once
#include <cstdint>
#include <string>

#include "../editor/syntax.h"
#include "fonts.h"

#define LCD_W_MAX 320
#define LCD_H_MAX 320

struct Theme {
    const char* name;
    uint16_t bg, fg, dim, sel, cursor;
    uint16_t statusBg, statusFg;
    uint16_t panelBg, panelFg, panelHi, panelHiFg, accent;
    // Syntax colours, indexed by syn::Cls (syn[Plain] is unused: fg is used).
    uint16_t syn[syn::kClsCount];
};

struct Cell {
    uint16_t cp;  // code point (Latin-1 range rendered, others as '?')
    uint16_t fg;
    uint16_t bg;
};

namespace gfx {

constexpr int kThemeCount = 3;
const Theme& theme(int idx);

// Decode one UTF-8 sequence from s (len bytes available). Returns code point,
// sets *used to the number of bytes consumed (>= 1).
uint32_t decodeUtf8(const uint8_t* s, uint32_t len, int* used);

// Draws `n` cells at (x, y), filling a band `widthPx` wide with `bg` first.
// If cursorCol >= 0 a 2 px caret is drawn at the left edge of that cell.
void drawCells(int x, int y, int widthPx, const Font& f, const Cell* cells, int n,
               uint16_t bg, int cursorCol = -1, uint16_t cursorColor = 0);

// Draws a UTF-8 string, clipped to widthPx. Returns cells drawn.
int drawText(int x, int y, int widthPx, const Font& f, const std::string& s, uint16_t fg,
             uint16_t bg, int cursorCol = -1, uint16_t cursorColor = 0);

// Number of cells (characters) in a UTF-8 string.
int textCells(const std::string& s);

void fill(int x, int y, int w, int h, uint16_t color);
void frame(int x, int y, int w, int h, uint16_t color);

}  // namespace gfx
