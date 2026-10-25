// PicoCraft - the block world: storage, generation, block picking.
#pragma once
#include <cstdint>

#include "config.h"

enum {
    B_AIR = 0, B_GRASS, B_DIRT, B_STONE, B_COBBLE, B_LOG, B_LEAVES, B_PLANKS,
    B_SAND, B_WATER, B_GLASS, B_BRICK, B_BEDROCK, B_ORE, B_SNOW, B_WOOL,
    B_COUNT
};

// 4 bits per block, [y][z][x] order
extern uint8_t world[WY * WZ * WX / 2];
extern uint32_t world_seed;

static inline int wblock(int x, int y, int z) {
    if (y < 0) return B_BEDROCK;
    if (y >= WY) return B_AIR;
    x &= WMASK_X; z &= WMASK_Z;
    uint32_t i = ((uint32_t)y << (WX_BITS + WZ_BITS)) | ((uint32_t)z << WX_BITS) | (uint32_t)x;
    uint8_t v = world[i >> 1];
    return (i & 1) ? (v >> 4) : (v & 15);
}

static inline void wset(int x, int y, int z, int b) {
    if (y < 0 || y >= WY) return;
    x &= WMASK_X; z &= WMASK_Z;
    uint32_t i = ((uint32_t)y << (WX_BITS + WZ_BITS)) | ((uint32_t)z << WX_BITS) | (uint32_t)x;
    uint8_t* p = &world[i >> 1];
    if (i & 1) *p = (uint8_t)((*p & 0x0F) | (b << 4));
    else       *p = (uint8_t)((*p & 0xF0) | (b & 15));
}

static inline bool bsolid(int b) { return b != B_AIR && b != B_WATER; }

void world_generate(uint32_t seed);
int  world_surface(int x, int z);      // y of the first air block above the ground

// Block picking ray (reach in blocks). True if it hits a block.
bool world_pick(float ox, float oy, float oz, float dx, float dy, float dz, float reach,
                int* hx, int* hy, int* hz, int* nx, int* ny, int* nz);

const char* block_name(int b);
