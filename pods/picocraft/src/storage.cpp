// PicoCraft - world saves: the header, then the whole world array.
// The file format is the one of PicoCraft 1.7, so its saves still load.
#include "storage.h"

#include <cstdio>
#include <cstring>

#include "config.h"
#include "pd/fs.h"
#include "world.h"

namespace {

// The game does not need the card to start: it is mounted on first use.
bool mount() { return pd::fs::mounted() || pd::fs::mount(); }

void path(char* buf, size_t n, int slot) { snprintf(buf, n, SAVE_DIR "/world%d.pcw", slot + 1); }

}  // namespace

int storage_save(int slot, const save_hdr_t* h) {
    if (!mount()) return SAVE_NO_CARD;
    pd::fs::mkdirs(SAVE_DIR);
    char p[40];
    path(p, sizeof p, slot);
    pd::fs::File f;
    if (!f.open(p, pd::fs::Mode::Write)) {
        pd::fs::unmount();   // the card may have been swapped: mount again next time
        return SAVE_IO_ERROR;
    }
    bool ok = f.write(h, sizeof *h) == int(sizeof *h) && f.write(world, sizeof world) == int(sizeof world);
    f.close();
    return ok ? SAVE_OK : SAVE_IO_ERROR;
}

int storage_load(int slot, save_hdr_t* h) {
    if (!mount()) return SAVE_NO_CARD;
    char p[40];
    path(p, sizeof p, slot);
    if (!pd::fs::exists(p)) return SAVE_EMPTY;
    pd::fs::File f;
    if (!f.open(p, pd::fs::Mode::Read)) return SAVE_IO_ERROR;
    if (f.size() != sizeof *h + sizeof world) return SAVE_BAD_FILE;
    if (f.read(h, sizeof *h) != int(sizeof *h) || memcmp(h->magic, "PCW1", 4) != 0) return SAVE_BAD_FILE;
    if (f.read(world, sizeof world) != int(sizeof world)) return SAVE_IO_ERROR;
    return SAVE_OK;
}
