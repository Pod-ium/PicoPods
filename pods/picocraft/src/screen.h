// PicoCraft - sends the RW x RH RGB565 frame to the display, scaled 2x.
//
// A 2x frame in the PicoCalc panel's 18-bit wire format is 300 KB, more than
// the RAM left next to the world, so the frame goes out in bands of a few
// rows: one band is converted while the previous one is on its way (DMA).
// pump() is cheap enough to call between two rendered rows, so the panel
// keeps receiving while both cores render the next frame.
#pragma once
#include <cstdint>

#include "pd/hal/display.h"

namespace screen {

void init(pd::hal::Display& d);
// Starts sending fb (RW*RH pixels). Finishes the frame in flight first. fb
// must not change until the frame is out (busy() false).
void start(const uint16_t* fb);
// Moves the transfer along; returns at once if the display is still busy.
void pump();
// Blocks until the frame is out.
void finish();
bool busy();

}  // namespace screen
