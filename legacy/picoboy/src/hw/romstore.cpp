// PicoBoy - ROM cache in flash
#include "romstore.h"
#include "../config.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/structs/qmi.h"
#include <string.h>
extern "C" {
#include "ff.h"
}

extern "C" char __flash_binary_end;

static constexpr uint32_t BLOCK = 65536;
static constexpr uint32_t MAGIC = 0x50424F59;   // "PBOY"

struct Meta {
    uint32_t magic;
    uint32_t size;
    uint32_t fdate_ftime;
    uint32_t checksum;         // of the header bytes, guards against stale data
    char path[240];
};
static_assert(sizeof(Meta) <= FLASH_PAGE_SIZE, "meta must fit a page");

static uint32_t phys_base;     // physical flash offset that runtime offset 0 maps to
static uint32_t data_off;      // runtime offset of ROM data (64 KB aligned)
static uint32_t meta_off;      // runtime offset of the metadata sector
static uint32_t capacity;

void romstore_init() {
    // With UF2 Loader on RP2350, QMI address translation window 0 maps 0x10000000 to the
    // start of the app partition and limits its size. Without it, the window is identity.
    uint32_t at = qmi_hw->atrans[0];
    uint32_t base_sect = (at & QMI_ATRANS0_BASE_BITS) >> QMI_ATRANS0_BASE_LSB;
    uint32_t size_sect = (at & QMI_ATRANS0_SIZE_BITS) >> QMI_ATRANS0_SIZE_LSB;
    phys_base = base_sect * FLASH_SECTOR_SIZE;
    uint32_t visible = size_sect * FLASH_SECTOR_SIZE;
    if (visible == 0 || visible > PICO_FLASH_SIZE_BYTES - phys_base) visible = PICO_FLASH_SIZE_BYTES - phys_base;

    uint32_t end = visible & ~(BLOCK - 1);
    meta_off = end - FLASH_SECTOR_SIZE;
    uint32_t bin_end = ((uint32_t)(uintptr_t)&__flash_binary_end - XIP_BASE + BLOCK - 1) & ~(BLOCK - 1);
    uint32_t want = end > ROMCACHE_MAX_BYTES ? ((end - ROMCACHE_MAX_BYTES) & ~(BLOCK - 1)) : 0;
    data_off = want > bin_end ? want : bin_end;
    capacity = meta_off > data_off ? meta_off - data_off : 0;
}

uint32_t romstore_capacity() { return capacity; }

static inline const uint8_t* xip(uint32_t runtime_off) { return (const uint8_t*)(XIP_BASE + runtime_off); }

struct EraseArgs { uint32_t off, len; };
struct ProgArgs { uint32_t off; const uint8_t* data; uint32_t len; };
static void do_erase(void* p) { auto* a = (EraseArgs*)p; flash_range_erase(phys_base + a->off, a->len); }
static void do_prog(void* p) { auto* a = (ProgArgs*)p; flash_range_program(phys_base + a->off, a->data, a->len); }

static bool erase(uint32_t off, uint32_t len) { EraseArgs a{off, len}; return flash_safe_execute(do_erase, &a, 1000) == PICO_OK; }
static bool program(uint32_t off, const uint8_t* d, uint32_t len) { ProgArgs a{off, d, len}; return flash_safe_execute(do_prog, &a, 1000) == PICO_OK; }

static uint32_t header_sum(const uint8_t* rom) {
    uint32_t s = 0;
    for (int i = 0x100; i < 0x150; i++) s = s * 31 + rom[i];
    return s;
}

const uint8_t* romstore_load(const char* path, uint32_t* size_out, RomProgressFn progress, void* user,
                             const char** err) {
    static FIL f;
    static FILINFO fi;
    static uint8_t buf[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));
    *err = nullptr;

    if (f_stat(path, &fi) != FR_OK) { *err = "Cannot read file"; return nullptr; }
    uint32_t size = (uint32_t)fi.fsize;
    if (size < 0x8000) { *err = "File too small to be a ROM"; return nullptr; }
    if (size > capacity) { *err = "ROM too large for flash cache"; return nullptr; }
    uint32_t stamp = ((uint32_t)fi.fdate << 16) | fi.ftime;

    if (f_open(&f, path, FA_READ) != FR_OK) { *err = "Cannot open file"; return nullptr; }
    UINT got = 0;
    if (f_read(&f, buf, 0x150, &got) != FR_OK || got != 0x150) { f_close(&f); *err = "Read error"; return nullptr; }

    // Already cached?
    const Meta* m = (const Meta*)xip(meta_off);
    if (m->magic == MAGIC && m->size == size && m->fdate_ftime == stamp && m->checksum == header_sum(buf) &&
        strncmp(m->path, path, sizeof(m->path)) == 0 && header_sum(xip(data_off)) == m->checksum) {
        f_close(&f);
        *size_out = size;
        return xip(data_off);
    }

    // Invalidate first, so an interrupted copy is never mistaken for a valid cache
    if (!erase(meta_off, FLASH_SECTOR_SIZE)) { f_close(&f); *err = "Flash erase failed"; return nullptr; }
    f_lseek(&f, 0);

    uint32_t padded = (size + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
    for (uint32_t off = 0; off < padded; off += FLASH_SECTOR_SIZE) {
        if ((off % BLOCK) == 0) {
            uint32_t len = padded - off >= BLOCK ? BLOCK : ((padded - off + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1));
            if (!erase(data_off + off, len)) { f_close(&f); *err = "Flash erase failed"; return nullptr; }
        }
        memset(buf, 0xFF, sizeof buf);
        if (f_read(&f, buf, FLASH_SECTOR_SIZE, &got) != FR_OK) { f_close(&f); *err = "SD read error"; return nullptr; }
        if (!program(data_off + off, buf, FLASH_SECTOR_SIZE)) { f_close(&f); *err = "Flash write failed"; return nullptr; }
        if (progress) progress(off + FLASH_SECTOR_SIZE, padded, user);
    }
    f_close(&f);

    static Meta nm __attribute__((aligned(4)));
    static uint8_t page[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
    memset(&nm, 0, sizeof nm);
    nm.magic = MAGIC;
    nm.size = size;
    nm.fdate_ftime = stamp;
    nm.checksum = header_sum(xip(data_off));
    strncpy(nm.path, path, sizeof(nm.path) - 1);
    memset(page, 0xFF, sizeof page);
    memcpy(page, &nm, sizeof nm);
    if (!program(meta_off, page, FLASH_PAGE_SIZE)) { *err = "Flash write failed"; return nullptr; }

    *size_out = size;
    return xip(data_off);
}
