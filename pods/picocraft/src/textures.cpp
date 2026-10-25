// PicoCraft - 16x16 textures generated at start-up (no external assets).
#include "textures.h"

#include <cmath>

uint16_t tex[B_COUNT][4][256];

typedef struct { int r, g, b, a; } px_t;   // a = 0: transparent

static uint32_t th(int x, int y, int k) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)k * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static float rnd(int x, int y, int k) { return (th(x, y, k) & 0xFFFF) / 65535.0f; }

static px_t col(int r, int g, int b, float m) {
    px_t p = {(int)(r * m), (int)(g * m), (int)(b * m), 1};
    return p;
}

// face: 0 top, 1 side, 2 bottom
static px_t texel(int blk, int face, int x, int y) {
    float n = rnd(x, y, blk * 3 + face);
    float n2 = rnd(x >> 1, y >> 1, blk * 7 + 100);
    switch (blk) {
    case B_GRASS:
        if (face == 0) return col(95, 159, 53, 0.78f + n * 0.3f);
        if (face == 2) return col(134, 96, 67, 0.8f + n * 0.3f);
        if (y < 3 + (int)(rnd(x, 0, 5) * 2.2f)) return col(95, 159, 53, 0.75f + n * 0.3f);
        return col(134, 96, 67, 0.8f + n * 0.3f);
    case B_DIRT:
        return col(134, 96, 67, (n2 < 0.12f ? 0.65f : 0.82f) + n * 0.25f);
    case B_STONE:
        return col(125, 125, 125, 0.82f + n * 0.12f + n2 * 0.1f);
    case B_COBBLE: {
        // Voronoi-like cells on a periodic 4x4 grid
        float best = 99, second = 99; int id = 0;
        for (int cy = -1; cy <= 4; cy++)
            for (int cx = -1; cx <= 4; cx++) {
                int wx = (cx + 4) & 3, wy = (cy + 4) & 3;
                float fx = cx * 4 + rnd(wx, wy, 77) * 4, fy = cy * 4 + rnd(wx, wy, 78) * 4;
                float d = (fx - x - 0.5f) * (fx - x - 0.5f) + (fy - y - 0.5f) * (fy - y - 0.5f);
                if (d < best) { second = best; best = d; id = wx + wy * 4; }
                else if (d < second) second = d;
            }
        if (sqrtf(second) - sqrtf(best) < 0.9f) return col(70, 70, 70, 0.9f + n * 0.1f);
        return col(128, 128, 128, 0.8f + rnd(id, 0, 79) * 0.3f + n * 0.08f);
    }
    case B_LOG:
        if (face != 1) {
            float dx = x - 7.5f, dy = y - 7.5f;
            float d = sqrtf(dx * dx + dy * dy);
            if (d > 6.6f) return col(102, 81, 50, 0.8f + n * 0.2f);
            return ((int)d & 1) ? col(170, 138, 86, 0.9f + n * 0.1f) : col(140, 110, 66, 0.9f + n * 0.1f);
        }
        return col(102, 81, 50, 0.7f + rnd(x, 0, 31) * 0.35f + n * 0.1f);
    case B_LEAVES:
        if (n < 0.22f) { px_t p = {0, 0, 0, 0}; return p; }
        return col(60, 135, 42, 0.6f + n2 * 0.3f + n * 0.25f);
    case B_PLANKS: {
        int plank = y >> 2;
        int seam = (int)(rnd(plank, 0, 41) * 16);
        if ((y & 3) == 3 || x == seam) return col(110, 85, 50, 0.95f);
        return col(170, 137, 84, 0.85f + rnd(x >> 2, y, 42) * 0.1f + n * 0.08f);
    }
    case B_SAND:
        return col(219, 207, 163, 0.88f + n * 0.12f);
    case B_WATER:
        return col(48, 96, 210, 0.85f + 0.1f * sinf((x + y * 0.5f) * 0.8f) + n * 0.08f);
    case B_GLASS:
        if (x == 0 || y == 0 || x == 15 || y == 15) return col(205, 232, 240, 1.0f);
        if ((x - y == 3 || x - y == 4) && x > 3 && x < 9) return col(230, 245, 250, 1.0f);
        { px_t p = {0, 0, 0, 0}; return p; }
    case B_BRICK: {
        int row = y >> 2;
        int off = (row & 1) ? 4 : 0;
        if ((y & 3) == 3 || ((x + off) & 7) == 7) return col(185, 178, 168, 0.85f + n * 0.1f);
        return col(155, 72, 55, 0.85f + n * 0.15f);
    }
    case B_BEDROCK:
        return col(85, 85, 85, 0.35f + n * 0.6f);
    case B_ORE:
        if (rnd(x >> 1, y >> 1, 55) > 0.78f && n > 0.25f) return col(250, 212, 70, 0.85f + n * 0.15f);
        return col(125, 125, 125, 0.82f + n * 0.12f + n2 * 0.1f);
    case B_SNOW:
        if (face == 0) return col(240, 246, 252, 0.93f + n * 0.07f);
        if (face == 2) return col(134, 96, 67, 0.8f + n * 0.3f);
        if (y < 4 + (int)(rnd(x, 0, 6) * 2.2f)) return col(240, 246, 252, 0.92f + n * 0.08f);
        return col(134, 96, 67, 0.8f + n * 0.3f);
    case B_WOOL:
        return col(200, 52, 48, 0.85f + ((x + y) & 1) * 0.06f + n * 0.08f);
    default:
        return col(255, 0, 255, 1);
    }
}

void textures_init(void) {
    static const float shade[4] = {1.0f, 0.82f, 0.68f, 0.55f};
    for (int b = 0; b < B_COUNT; b++)
        for (int s = 0; s < 4; s++) {
            int face = (s == 0) ? 0 : (s == 3 ? 2 : 1);
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) {
                    px_t p = texel(b, face, x, y);
                    uint16_t c;
                    if (!p.a) c = TEX_CLEAR;
                    else {
                        c = rgb565((int)(p.r * shade[s]), (int)(p.g * shade[s]), (int)(p.b * shade[s]));
                        if (c == TEX_CLEAR) c = 0;
                    }
                    tex[b][s][y * 16 + x] = c;
                }
        }
}
