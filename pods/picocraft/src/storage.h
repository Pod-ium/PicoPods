// PicoCraft - world saves on the SD card: SAVE_DIR/worldN.pcw.
#pragma once
#include <cstdint>

struct save_hdr_t {
    char magic[4];          // "PCW1"
    uint32_t seed;
    float x, y, z, yaw, pitch;
    uint8_t fly, slot, view_dist, pad;
    uint8_t hotbar[9];
    uint8_t reserved[23];
};

enum { SAVE_OK = 0, SAVE_NO_CARD, SAVE_EMPTY, SAVE_IO_ERROR, SAVE_BAD_FILE };

// SAVE_OK or an error above. A failed load may leave the world half read.
int storage_save(int slot, const save_hdr_t* h);
int storage_load(int slot, save_hdr_t* h);
