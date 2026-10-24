// picodev terminal - "host": runs picodev apps on a PC, headless.
//
// The display is an RGB framebuffer that can be written out as PPM images, the
// keyboard plays a script, the SD card is a folder, audio is discarded (but
// paced in real time, so games that sync to audio run at normal speed).
//
// Environment:
//   PD_KEYS    key script, comma separated (default: read tokens from stdin)
//              tokens: up down left right enter esc bksp tab space del ins home
//              end pgup pgdn f1..f10 comma, a single character ("a", "A", "?"),
//              0xNN, ctrl+x / shift+x / alt+x, text:hello (types the text),
//              wait:250 (ms), shot (dump a frame now)
//              set but empty: no key ever arrives (for tests that inject keys)
//   PD_FRAMES  folder for frame dumps: one PPM each time a key token starts,
//              and one at exit (default: none)
//   PD_SDCARD  folder used as the SD card root (default: ./sdcard)
//   PD_SCREEN  WxH (default: 320x320)
// When the script runs out the program exits with status 0.
#pragma once
#include <string>

#include "pd/terminal.h"

namespace pd::host {

bool dump_frame(const std::string& path);   // writes the framebuffer as PPM
const uint8_t* framebuffer();                // RGB888, width*height*3
// Pixel at (x, y) as 0xRRGGBB (tests).
Color pixel(int x, int y);
// Number of stereo frames the audio output has received.
uint64_t audio_frames_played();

}  // namespace pd::host
