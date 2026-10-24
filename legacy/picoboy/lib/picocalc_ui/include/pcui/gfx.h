// picocalc_ui - text and rectangle drawing (no framebuffer; one band buffer).
#pragma once
#include <cstdint>
#include "pcui/lcd.h"

namespace pcui {

struct Font {
    uint8_t w, h, bpr;
    const uint8_t* data;
};

namespace gfx {

enum FontId { FONT_6x12 = 0, FONT_8x16 = 1, FONT_10x20 = 2, FONT_12x24 = 3 };
const Font& font(int id);

// Number of characters in a UTF-8 string (Latin-1 is rendered, the rest as '?').
int text_cells(const char* s);

// Draws s at (x, y). The band [x, x + width_px) is filled with bg first; if
// width_px <= 0 the band is exactly as wide as the text. Returns cells drawn.
int text(int x, int y, int width_px, const Font& f, const char* s, Color fg, Color bg);

void fill(int x, int y, int w, int h, Color c);
void frame(int x, int y, int w, int h, Color c);   // 1 px outline

}  // namespace gfx
}  // namespace pcui
