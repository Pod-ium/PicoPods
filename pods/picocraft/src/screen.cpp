// PicoCraft - banded 2x output of the frame (see screen.h).
#include "screen.h"

#include <cstring>

#include "config.h"

namespace screen {

namespace {

constexpr int kScale = 2;
constexpr int kOutW = RW * kScale;
constexpr int kBandRows = 8;                 // source rows per band
constexpr int kBands = RH / kBandRows;
static_assert(RH % kBandRows == 0, "RH must be a multiple of the band height");

pd::hal::Display* disp = nullptr;
int bpp = 3;
size_t band_bytes = 0;
uint8_t band[2][kBandRows * kScale * kOutW * 3];   // 2 x 15 KB

const uint16_t* src = nullptr;
int converted = 0;   // bands converted so far (at most one ahead of sent)
int sent = 0;        // bands handed to the display
bool active = false;

// One source row to one panel line (each pixel twice), RGB565 to the wire format.
void expand_row(const uint16_t* s, uint8_t* d) {
    if (bpp == 3) {   // RGB666 panel / RGB888 host: R, G, B bytes, low bits replicated
        for (int x = 0; x < RW; ++x) {
            const uint16_t c = s[x];
            uint8_t r = uint8_t((c >> 8) & 0xF8), g = uint8_t((c >> 3) & 0xFC), b = uint8_t(c << 3);
            r |= r >> 5;
            g |= g >> 6;
            b |= b >> 5;
            d[0] = d[3] = r;
            d[1] = d[4] = g;
            d[2] = d[5] = b;
            d += 6;
        }
    } else {          // RGB565 big endian
        for (int x = 0; x < RW; ++x) {
            const uint8_t hi = uint8_t(s[x] >> 8), lo = uint8_t(s[x]);
            d[0] = d[2] = hi;
            d[1] = d[3] = lo;
            d += 4;
        }
    }
}

void convert(int b, uint8_t* out) {
    const size_t line = size_t(kOutW) * size_t(bpp);
    for (int r = 0; r < kBandRows; ++r) {
        uint8_t* l = out + size_t(r) * kScale * line;
        expand_row(src + (b * kBandRows + r) * RW, l);
        memcpy(l + line, l, line);
    }
}

void convert_ahead() {
    if (converted == sent && converted < kBands) {
        convert(converted, band[converted & 1]);
        ++converted;
    }
}

}  // namespace

void init(pd::hal::Display& d) {
    disp = &d;
    bpp = d.bpp();
    band_bytes = size_t(kBandRows) * kScale * kOutW * size_t(bpp);
}

void start(const uint16_t* fb) {
    finish();
    src = fb;
    converted = sent = 0;
    active = true;
    const int x0 = (disp->width() - kOutW) / 2, y0 = (disp->height() - RH * kScale) / 2;
    disp->begin(x0, y0, x0 + kOutW - 1, y0 + RH * kScale - 1);
    pump();
}

void pump() {
    if (!active) return;
    if (disp->busy()) {   // the band in flight uses the other buffer
        convert_ahead();
        return;
    }
    if (sent == kBands) {
        disp->end();
        active = false;
        return;
    }
    convert_ahead();
    disp->write_async(band[sent & 1], band_bytes);
    ++sent;
    convert_ahead();
}

void finish() {
    while (active) pump();
}

bool busy() { return active; }

}  // namespace screen
