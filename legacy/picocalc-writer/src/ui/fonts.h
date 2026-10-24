// Fonts come from the shared picocalc_ui library (one copy in flash, see
// lib/picocalc_ui/src/fonts_terminus.h). This header keeps Writer's names:
// Font, kFonts[i] (a const Font*) and kFontCount.
// Terminus Font (c) Dimitar Toshkov Zhekov, SIL Open Font License 1.1
#pragma once
#include "pcui/gfx.h"

using Font = pcui::Font;

constexpr int kFontCount = 4;  // 6x12, 8x16, 10x20, 12x24

struct FontTable {
    const Font* operator[](int i) const { return &pcui::gfx::font(i); }
};
inline constexpr FontTable kFonts{};
