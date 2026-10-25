// PicoCraft - game configuration. Hardware (pins, SPI/I2C speeds) lives in
// the PicoPods PicoCalc terminal: override it with PD_PICOCALC_* compile
// definitions in CMakeLists.txt (see docs/PORTING.md).
#pragma once

// ---------------------------------------------------------------- clocks
#define SYS_CLOCK_KHZ        200000   // RP2350 overclock; 150000 is stock. Drop if unstable.

// ---------------------------------------------------------------- rendering
// The world is ray-cast at RW x RH and scaled 2x to the 320x320 panel.
#define RW                   160
#define RH                   160

// ---------------------------------------------------------------- world
// Toroidal world (the edges wrap around) 128 x 128, 32 blocks high,
// 4 bits per block = 256 KB of RAM.
#define WX_BITS              7
#define WZ_BITS              7
#define WY                   32
#define WX                   (1 << WX_BITS)
#define WZ                   (1 << WZ_BITS)
#define WMASK_X              (WX - 1)
#define WMASK_Z              (WZ - 1)
#define SEA_LEVEL            11

// View distance in blocks (adjustable from the menu)
#define VIEW_DIST_DEFAULT    20
#define VIEW_DIST_MIN        10
#define VIEW_DIST_MAX        32

// ---------------------------------------------------------------- saves
#define SAVE_DIR             "/picocraft"
#define SAVE_SLOTS           3
