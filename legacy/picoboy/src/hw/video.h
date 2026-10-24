// PicoBoy - streams Game Boy frames to the LCD, scaled 2x (320x288, centred vertically).
// Each frame goes out as one DMA transfer of only the lines that changed.
#pragma once
#include <stdint.h>

constexpr int GAME_Y = 16;          // top of the 320x288 game area
constexpr int GAME_H = 288;

void video_init();
void video_set_palette(const uint32_t rgb[4]);    // shade 0 (lightest) .. 3 (darkest)
bool video_busy();
bool video_submit(const uint8_t* fb);             // true if accepted (sent, or nothing changed)
void video_poll();                                // closes a finished transfer; cheap
void video_invalidate();                          // next submit redraws everything (after menus)
void video_wait_idle();                           // polls until the current frame is done
