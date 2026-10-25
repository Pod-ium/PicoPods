// PicoCraft - voxel ray caster (one DDA ray per pixel, "Minecraft4k" style),
// shared between the two cores.
#include "render.h"

#include <atomic>
#include <cmath>

#include "textures.h"
#include "world.h"

#if PICOCRAFT_DEVICE
#include "pico.h"            // __not_in_flash_func: the hot code runs from RAM
#else
#define __not_in_flash_func(f) f
#endif

#define FOV_TAN 0.80f    // tan(FOV/2): ~77 degrees

static struct {
    float ox, oy, oz;
    float fx, fy, fz, rx, ry, rz, ux, uy, uz;
    float maxd, fog0, fog_inv;
    int sel, selx, sely, selz;
    int underwater;
    uint16_t fogc;
    uint16_t *fb;
} R;

static float sxt[RW], syt[RH];
static uint16_t sky_lut[64];
static int tables_ready;
static std::atomic<int> next_row{RH};

static inline uint16_t blend565(uint16_t a, uint16_t b, int f) {
    uint32_t A = (a | ((uint32_t)a << 16)) & 0x07E0F81Fu;
    uint32_t B = (b | ((uint32_t)b << 16)) & 0x07E0F81Fu;
    uint32_t r = ((A * (uint32_t)(32 - f) + B * (uint32_t)f) >> 5) & 0x07E0F81Fu;
    return (uint16_t)(r | (r >> 16));
}

static void init_tables(void) {
    for (int i = 0; i < RW; i++) sxt[i] = ((i + 0.5f) / RW * 2 - 1) * FOV_TAN;
    for (int i = 0; i < RH; i++) syt[i] = (1 - (i + 0.5f) / RH * 2) * FOV_TAN;
    for (int i = 0; i < 64; i++) {
        float t = i / 63.0f;               // 0 = looking down, 1 = looking up
        float k = (t - 0.5f) * 2.0f;
        if (k < 0) k = 0;
        k = sqrtf(k);
        sky_lut[i] = rgb565((int)(172 - 85 * k), (int)(212 - 62 * k), (int)(250 - 18 * k));
    }
    tables_ready = 1;
}

void render_setup(const camera_t *c) {
    if (!tables_ready) init_tables();
    float cy = cosf(c->yaw), sy = sinf(c->yaw), cp = cosf(c->pitch), sp = sinf(c->pitch);
    R.ox = c->x; R.oy = c->y; R.oz = c->z;
    // yaw 0 looks towards -Z and grows to the right
    R.fx = sy * cp;  R.fy = sp;  R.fz = -cy * cp;
    R.rx = cy;       R.ry = 0;   R.rz = sy;
    R.ux = -sy * sp; R.uy = cp;  R.uz = cy * sp;
    R.underwater = c->underwater;
    if (c->underwater) {
        R.maxd = 9.0f;
        R.fogc = rgb565(30, 60, 140);
    } else {
        R.maxd = (float)c->view_dist;
        R.fogc = sky_lut[32];
    }
    R.fog0 = R.maxd * 0.55f;
    R.fog_inv = 32.0f / (R.maxd - R.fog0);
    R.sel = c->sel;
    R.selx = c->sx & WMASK_X; R.sely = c->sy; R.selz = c->sz & WMASK_Z;
    R.fb = c->fb;
    next_row.store(0, std::memory_order_release);
}

static inline int fastblock(int x, int y, int z) {
    uint32_t i = ((uint32_t)y << (WX_BITS + WZ_BITS)) | ((uint32_t)(z & WMASK_Z) << WX_BITS) | (uint32_t)(x & WMASK_X);
    uint8_t v = world[i >> 1];
    return (i & 1) ? (v >> 4) : (v & 15);
}

static uint16_t __not_in_flash_func(trace)(float dx, float dy, float dz) {
    const float ox = R.ox, oy = R.oy, oz = R.oz;
    int ix = (int)floorf(ox), iy = (int)floorf(oy), iz = (int)floorf(oz);
    const int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1, sz = dz > 0 ? 1 : -1;
    const float tdx = fabsf(1.0f / dx), tdy = fabsf(1.0f / dy), tdz = fabsf(1.0f / dz);
    float tx = (dx > 0 ? (ix + 1 - ox) : (ox - ix)) * tdx;
    float ty = (dy > 0 ? (iy + 1 - oy) : (oy - iy)) * tdy;
    float tz = (dz > 0 ? (iz + 1 - oz) : (oz - iz)) * tdz;
    const float maxd = R.maxd;

    int in_water = R.underwater;
    float w_enter = 0, w_depth = 0;
    uint16_t w_col = rgb565(40, 80, 190);
    int had_water = in_water;

    uint16_t c = 0;
    float t = 0;
    int hit = 0;

    for (int n = 0; n < 96; n++) {
        int face;
        if (tx < ty) {
            if (tx < tz) { t = tx; ix += sx; tx += tdx; face = 0; }
            else         { t = tz; iz += sz; tz += tdz; face = 2; }
        } else {
            if (ty < tz) { t = ty; iy += sy; ty += tdy; face = 1; }
            else         { t = tz; iz += sz; tz += tdz; face = 2; }
        }
        if (t > maxd) break;
        if ((unsigned)iy >= WY) {
            if (iy < 0) { c = rgb565(30, 30, 30); hit = 1; }
            break;                                    // above the world: sky
        }
        int b = fastblock(ix, iy, iz);
        if (b == B_AIR) {
            if (in_water) { w_depth += t - w_enter; in_water = 0; }
            continue;
        }
        if (b == B_WATER) {
            if (!in_water) {
                in_water = 1; w_enter = t;
                if (!had_water) {
                    // colour of the water surface where the ray enters it
                    float hx = ox + dx * t, hz = oz + dz * t;
                    int u = (int)((hx - floorf(hx)) * 16) & 15, v = (int)((hz - floorf(hz)) * 16) & 15;
                    w_col = tex[B_WATER][face == 1 ? 0 : face][v * 16 + u];
                    had_water = 1;
                }
            }
            continue;
        }
        // texture coordinates
        int u, v, shade;
        if (face == 0) {
            float hz = oz + dz * t, hy = oy + dy * t;
            u = (int)((hz - iz) * 16); v = 15 - (int)((hy - iy) * 16);
            if (sx < 0) u = 15 - u;
            shade = 1;
        } else if (face == 2) {
            float hx = ox + dx * t, hy = oy + dy * t;
            u = (int)((hx - ix) * 16); v = 15 - (int)((hy - iy) * 16);
            if (sz > 0) u = 15 - u;
            shade = 2;
        } else {
            float hx = ox + dx * t, hz = oz + dz * t;
            u = (int)((hx - ix) * 16); v = (int)((hz - iz) * 16);
            shade = sy < 0 ? 0 : 3;
        }
        u &= 15; v &= 15;
        uint16_t tc = tex[b][shade][v * 16 + u];
        if (tc == TEX_CLEAR) continue;                 // leaves/glass: keep going
        if (in_water) { w_depth += t - w_enter; in_water = 0; }
        c = tc;
        if (R.sel && iy == R.sely && (ix & WMASK_X) == R.selx && (iz & WMASK_Z) == R.selz &&
            (u == 0 || u == 15 || v == 0 || v == 15))
            c = (c >> 2) & 0x39E7;                     // outline of the targeted block
        hit = 1;
        break;
    }

    if (!hit) {
        if (in_water) w_depth += maxd - w_enter;
        c = R.underwater ? R.fogc : sky_lut[(int)((dy * 0.5f + 0.5f) * 63.0f) & 63];
        t = maxd;
    } else if (t > R.fog0) {
        int f = (int)((t - R.fog0) * R.fog_inv);
        if (f > 32) f = 32;
        c = blend565(c, R.fogc, f);
    }
    if (R.underwater) {
        int f = (int)(t * 2.0f) + 8;                  // blue tint under water
        if (f > 32) f = 32;
        c = blend565(c, R.fogc, f);
    } else if (had_water) {
        int f = 13 + (int)(w_depth * 2.5f);
        if (f > 28) f = 28;
        c = blend565(c, w_col, f);
    }
    return c;
}

static void __not_in_flash_func(render_row)(int y) {
    float sy = syt[y];
    float bx = R.fx + R.ux * sy, by = R.fy + R.uy * sy, bz = R.fz + R.uz * sy;
    uint16_t *out = R.fb + y * RW;
    for (int x = 0; x < RW; x++) {
        float s = sxt[x];
        float dx = bx + R.rx * s, dy = by, dz = bz + R.rz * s;
        float inv = 1.0f / sqrtf(dx * dx + dy * dy + dz * dz);
        dx *= inv; dy *= inv; dz *= inv;
        // no division by an exact zero
        if (dx == 0) dx = 1e-6f;
        if (dy == 0) dy = 1e-6f;
        if (dz == 0) dz = 1e-6f;
        out[x] = trace(dx, dy, dz);
    }
}

void render_rows(int row0, int step) {
    for (int y = row0; y < RH; y += step) render_row(y);
}

void __not_in_flash_func(render_claimed)(void (*between)()) {
    for (;;) {
        const int y = next_row.fetch_add(1, std::memory_order_relaxed);
        if (y >= RH) return;
        render_row(y);
        if (between) between();
    }
}
