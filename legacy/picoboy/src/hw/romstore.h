// PicoBoy - keeps the currently selected ROM in flash (memory-mapped, read via XIP).
//
// Works both when flashed directly (BOOTSEL/picotool) and when launched by the
// PicoCalc UF2 Loader, which on the RP2350 runs the app from a flash partition
// with address translation. The cache always lives at the top of the flash
// window this app can see, above the firmware image.
#pragma once
#include <stdint.h>

typedef void (*RomProgressFn)(uint32_t done, uint32_t total, void* user);

void romstore_init();
uint32_t romstore_capacity();

// Copies path from the SD card into flash, unless the cache already holds that exact file.
// Returns a pointer to the memory-mapped ROM, or nullptr (err receives a message).
const uint8_t* romstore_load(const char* path, uint32_t* size_out, RomProgressFn progress, void* user,
                             const char** err);
