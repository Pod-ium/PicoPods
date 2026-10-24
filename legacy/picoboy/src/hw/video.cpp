// PicoBoy - frame streaming to the LCD
//
// The whole scaled frame (320x288, 18-bit) is converted into one RAM buffer and
// sent with a single DMA transfer, so no interrupts and no CPU help are needed
// while it is on the wire. Only the band of lines that changed since the last
// transfer is converted and sent, which makes mostly-static scenes much faster.
#include "video.h"
#include "pcui/lcd.h"
#include "../gb/gb.h"
#include "pico/stdlib.h"
#include <string.h>

using namespace pcui;

static constexpr int ROW_BYTES = SCREEN_W * lcd::BPP;      // one panel row (960 bytes)
static constexpr int GB_LINE_BYTES = ROW_BYTES * 2;        // one Game Boy line, scaled 2x

static uint8_t pal[4][3];
static uint8_t panel[gb::SCREEN_H * GB_LINE_BYTES] __attribute__((aligned(4)));   // 276 KB
static uint8_t sent[gb::SCREEN_W * gb::SCREEN_H];   // what the panel currently shows
static bool sent_valid = false;
static bool busy = false;

static void __not_in_flash_func(convert_line)(const uint8_t* fb, int y) {
    const uint8_t* src = fb + y * gb::SCREEN_W;
    uint8_t* row = panel + y * GB_LINE_BYTES;
    uint8_t* o = row;
    for (int x = 0; x < gb::SCREEN_W; x++) {
        const uint8_t* p = pal[src[x] & 3];
        o[0] = p[0]; o[1] = p[1]; o[2] = p[2]; o[3] = p[0]; o[4] = p[1]; o[5] = p[2];
        o += 6;
    }
    memcpy(row + ROW_BYTES, row, ROW_BYTES);   // vertical 2x
}

void video_init() {}

void video_invalidate() { sent_valid = false; }

void video_set_palette(const uint32_t rgb[4]) {
    video_wait_idle();
    for (int i = 0; i < 4; i++) lcd::encode(rgb[i], pal[i]);
    sent_valid = false;
}

void video_poll() {
    if (busy && !lcd::dma_busy()) {
        lcd::end();
        busy = false;
    }
}

bool video_busy() {
    video_poll();
    return busy;
}

bool video_submit(const uint8_t* fb) {
    video_poll();
    if (busy) return false;

    int first = 0, last = gb::SCREEN_H - 1;
    if (sent_valid) {
        while (first < gb::SCREEN_H && memcmp(fb + first * gb::SCREEN_W, sent + first * gb::SCREEN_W, gb::SCREEN_W) == 0) first++;
        if (first == gb::SCREEN_H) return true;   // nothing changed
        while (last > first && memcmp(fb + last * gb::SCREEN_W, sent + last * gb::SCREEN_W, gb::SCREEN_W) == 0) last--;
    }
    for (int y = first; y <= last; y++) convert_line(fb, y);
    memcpy(sent + first * gb::SCREEN_W, fb + first * gb::SCREEN_W, (last - first + 1) * gb::SCREEN_W);
    sent_valid = true;

    lcd::set_window(0, GAME_Y + first * 2, SCREEN_W - 1, GAME_Y + last * 2 + 1);
    busy = true;
    lcd::write_dma(panel + first * GB_LINE_BYTES, (size_t)(last - first + 1) * GB_LINE_BYTES);
    return true;
}

void video_wait_idle() {
    while (busy) video_poll();
}
