// PicoCraft - voxel ray caster.
#pragma once
#include <cstdint>

typedef struct {
    float x, y, z;          // eye position
    float yaw, pitch;       // radians
    int view_dist;          // blocks
    bool underwater;
    bool sel;               // outline the selected block
    int sx, sy, sz;
    uint16_t *fb;           // framebuffer RW x RH
} camera_t;

// Prepares a frame. Call before render_rows()/render_claimed().
void render_setup(const camera_t* cam);
// Draws rows row0, row0 + step, ...
void render_rows(int row0, int step);
// Draws rows taken one at a time from a counter shared by both cores (reset
// by render_setup), so each core does as much of the frame as it has time
// for. `between` (may be null) runs after each row. Returns when no rows are
// left; the other core may still be finishing its last one.
void render_claimed(void (*between)());
