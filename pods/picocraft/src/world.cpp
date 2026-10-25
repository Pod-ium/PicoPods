// PicoCraft - procedural world generation (toroidal: the noise is periodic).
#include "world.h"

#include <cmath>

uint8_t world[WY * WZ * WX / 2];
uint32_t world_seed;

static uint32_t hash3(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = world_seed ^ 0x9E3779B9u;
    h ^= a * 0x85EBCA6Bu; h = (h << 13) | (h >> 19); h *= 5; h += 0xE6546B64u;
    h ^= b * 0xC2B2AE35u; h = (h << 13) | (h >> 19); h *= 5; h += 0xE6546B64u;
    h ^= c * 0x27D4EB2Fu;
    h ^= h >> 16; h *= 0x85EBCA6Bu; h ^= h >> 13; h *= 0xC2B2AE35u; h ^= h >> 16;
    return h;
}

static float lattice(int ix, int iz, int period, int oct) {
    ix = ((ix % period) + period) % period;
    iz = ((iz % period) + period) % period;
    return (hash3((uint32_t)ix, (uint32_t)iz, (uint32_t)oct) & 0xFFFF) / 32767.5f - 1.0f;
}

// value noise with period WX/cell (wraps around at the world edges)
static float vnoise(float x, float z, int cell, int oct) {
    int period = WX / cell;
    float fx = x / cell, fz = z / cell;
    int ix = (int)floorf(fx), iz = (int)floorf(fz);
    float tx = fx - ix, tz = fz - iz;
    tx = tx * tx * (3 - 2 * tx);
    tz = tz * tz * (3 - 2 * tz);
    float a = lattice(ix, iz, period, oct), b = lattice(ix + 1, iz, period, oct);
    float c = lattice(ix, iz + 1, period, oct), d = lattice(ix + 1, iz + 1, period, oct);
    float ab = a + (b - a) * tx, cd = c + (d - c) * tx;
    return ab + (cd - ab) * tz;
}

static int height_at(int x, int z) {
    float h = 13.0f;
    h += vnoise(x, z, 32, 1) * 6.0f;
    h += vnoise(x, z, 16, 2) * 3.0f;
    h += vnoise(x, z, 8, 3) * 1.5f;
    float m = vnoise(x, z, 64, 4);
    if (m > 0) h += m * m * 22.0f;           // mountains
    int hi = (int)h;
    if (hi < 3) hi = 3;
    if (hi > WY - 4) hi = WY - 4;
    return hi;
}

static void place_tree(int x, int y, int z) {
    uint32_t r = hash3(uint32_t(x), uint32_t(z), 99);
    int th = 4 + (r & 1);
    if (y + th + 2 >= WY) return;
    for (int dy = th - 2; dy <= th + 1; dy++) {
        int rad = (dy >= th) ? 1 : 2;
        for (int dz = -rad; dz <= rad; dz++)
            for (int dx = -rad; dx <= rad; dx++) {
                bool corner = (dx == -rad || dx == rad) && (dz == -rad || dz == rad);
                if (corner && (rad == 1 || (hash3(x + dx, y + dy, z + dz) & 1))) continue;
                if (wblock(x + dx, y + dy, z + dz) == B_AIR) wset(x + dx, y + dy, z + dz, B_LEAVES);
            }
    }
    for (int i = 0; i < th; i++) wset(x, y + i, z, B_LOG);
}

void world_generate(uint32_t seed) {
    world_seed = seed;
    for (uint32_t i = 0; i < sizeof world; i++) world[i] = 0;

    for (int z = 0; z < WZ; z++)
        for (int x = 0; x < WX; x++) {
            int top = height_at(x, z);
            bool beach = top <= SEA_LEVEL + 1;
            bool high = top >= 22;
            for (int y = 0; y <= top; y++) {
                int b;
                if (y == 0) b = B_BEDROCK;
                else if (y < top - 3) {
                    b = B_STONE;
                    if (y < 14 && (hash3(x, y, z) % 90) == 0) b = B_ORE;
                } else if (y < top) b = beach ? B_SAND : (high && top >= 25 ? B_STONE : B_DIRT);
                else b = beach ? B_SAND : (high ? B_SNOW : B_GRASS);
                wset(x, y, z, b);
            }
            for (int y = top + 1; y <= SEA_LEVEL; y++) wset(x, y, z, B_WATER);
        }

    // small caves: tunnels dug by a random "worm"
    for (int w = 0; w < 10; w++) {
        uint32_t r = hash3(w, 7, 7);
        float px = r % WX, pz = (r >> 8) % WZ, py = 4 + (r >> 16) % 8;
        float ang = (r >> 4) * 0.001f, pitch = 0;
        for (int s = 0; s < 70; s++) {
            uint32_t q = hash3(w, s, 13);
            ang += ((int)(q & 255) - 128) * 0.004f;
            pitch = pitch * 0.8f + ((int)((q >> 8) & 255) - 128) * 0.002f;
            px += cosf(ang); pz += sinf(ang); py += pitch;
            if (py < 2) py = 2;
            if (py > 18) py = 18;
            int rad = 1 + ((q >> 20) & 1);
            for (int dy = -rad; dy <= rad; dy++)
                for (int dz = -rad; dz <= rad; dz++)
                    for (int dx = -rad; dx <= rad; dx++) {
                        if (dx * dx + dy * dy + dz * dz > rad * rad + 1) continue;
                        int bx = (int)px + dx, by = (int)py + dy, bz = (int)pz + dz;
                        int b = wblock(bx, by, bz);
                        if (b == B_STONE || b == B_DIRT || b == B_ORE) {
                            // keep the sea floor intact (no "floating" water)
                            if (wblock(bx, by + 1, bz) == B_WATER) continue;
                            wset(bx, by, bz, B_AIR);
                        }
                    }
        }
    }

    for (int z = 0; z < WZ; z++)
        for (int x = 0; x < WX; x++) {
            uint32_t r = hash3(x, z, 42);
            if (r % 70) continue;
            int y = world_surface(x, z);
            if (wblock(x, y - 1, z) != B_GRASS) continue;
            if (y <= SEA_LEVEL + 1) continue;
            wset(x, y - 1, z, B_DIRT);
            place_tree(x, y, z);
        }
}

int world_surface(int x, int z) {
    for (int y = WY - 1; y >= 0; y--) {
        int b = wblock(x, y, z);
        if (b != B_AIR && b != B_LEAVES) return y + 1;
    }
    return 1;
}

bool world_pick(float ox, float oy, float oz, float dx, float dy, float dz, float reach,
                int *hx, int *hy, int *hz, int *nx, int *ny, int *nz) {
    int ix = (int)floorf(ox), iy = (int)floorf(oy), iz = (int)floorf(oz);
    int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1, sz = dz > 0 ? 1 : -1;
    float tdx = dx != 0 ? fabsf(1.0f / dx) : 1e30f;
    float tdy = dy != 0 ? fabsf(1.0f / dy) : 1e30f;
    float tdz = dz != 0 ? fabsf(1.0f / dz) : 1e30f;
    float tx = (dx > 0 ? (ix + 1 - ox) : (ox - ix)) * tdx;
    float ty = (dy > 0 ? (iy + 1 - oy) : (oy - iy)) * tdy;
    float tz = (dz > 0 ? (iz + 1 - oz) : (oz - iz)) * tdz;
    int px = ix, py = iy, pz = iz;
    for (int i = 0; i < 64; i++) {
        float t;
        px = ix; py = iy; pz = iz;
        if (tx < ty && tx < tz) { t = tx; ix += sx; tx += tdx; }
        else if (ty < tz)       { t = ty; iy += sy; ty += tdy; }
        else                    { t = tz; iz += sz; tz += tdz; }
        if (t > reach) return false;
        if (iy < 0 || iy >= WY) return false;
        int b = wblock(ix, iy, iz);
        if (b != B_AIR && b != B_WATER) {
            *hx = ix; *hy = iy; *hz = iz;
            *nx = px; *ny = py; *nz = pz;
            return true;
        }
    }
    return false;
}

const char *block_name(int b) {
    static const char *n[B_COUNT] = {
        "AIR", "GRASS", "DIRT", "STONE", "COBBLESTONE", "LOG", "LEAVES", "PLANKS",
        "SAND", "WATER", "GLASS", "BRICKS", "BEDROCK", "GOLD ORE", "SNOW", "WOOL"};
    return (b >= 0 && b < B_COUNT) ? n[b] : "?";
}
