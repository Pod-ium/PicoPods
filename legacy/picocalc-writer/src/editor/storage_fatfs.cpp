// fs:: implemented on top of FatFs (device build).
#include "storage.h"

extern "C" {
#include "ff.h"
}

namespace fs {

namespace {
FATFS g_fs;
int g_err = 0;

inline bool check(FRESULT r) {
    if (r != FR_OK) g_err = int(r);
    return r == FR_OK;
}
}  // namespace

int lastError() { return g_err; }

const char* lastErrorText() {
    switch (g_err) {
        case FR_OK: return "no error";
        case FR_DISK_ERR: return "SD card I/O error";
        case FR_INT_ERR: return "file system error";
        case FR_NOT_READY: return "SD card not ready";
        case FR_NO_FILE: return "file not found";
        case FR_NO_PATH: return "folder not found";
        case FR_INVALID_NAME: return "invalid name";
        case FR_DENIED: return "access denied or disk full";
        case FR_EXIST: return "already exists";
        case FR_WRITE_PROTECTED: return "write protected";
        case FR_NOT_ENABLED: case FR_NO_FILESYSTEM: return "no file system";
        case FR_TIMEOUT: return "timeout";
        case FR_LOCKED: return "file locked";
        case FR_NOT_ENOUGH_CORE: return "out of memory";
        case FR_TOO_MANY_OPEN_FILES: return "too many open files";
        default: return "error";
    }
}

File::File() {}
File::~File() { close(); }

bool File::open(const char* path, Mode mode) {
    close();
    BYTE m = FA_READ | FA_OPEN_EXISTING;
    if (mode == Mode::ReadWrite) m = FA_READ | FA_WRITE | FA_CREATE_ALWAYS;
    if (mode == Mode::Write) m = FA_WRITE | FA_CREATE_ALWAYS;
    FIL* f = new FIL;
    if (!check(f_open(f, path, m))) {
        delete f;
        return false;
    }
    impl_ = f;
    open_ = true;
    return true;
}

void File::close() {
    if (open_) {
        FIL* f = static_cast<FIL*>(impl_);
        f_close(f);
        delete f;
    }
    open_ = false;
    impl_ = nullptr;
}

bool File::seek(uint32_t pos) { return check(f_lseek(static_cast<FIL*>(impl_), pos)); }

int File::read(void* buf, uint32_t n) {
    UINT br = 0;
    if (!check(f_read(static_cast<FIL*>(impl_), buf, n, &br))) return -1;
    return int(br);
}

int File::write(const void* buf, uint32_t n) {
    UINT bw = 0;
    if (!check(f_write(static_cast<FIL*>(impl_), buf, n, &bw))) return -1;
    return int(bw);
}

uint32_t File::size() { return uint32_t(f_size(static_cast<FIL*>(impl_))); }

bool File::sync() { return check(f_sync(static_cast<FIL*>(impl_))); }

bool mount() { return f_mount(&g_fs, "", 1) == FR_OK; }

bool exists(const char* path) {
    if (path[0] == '/' && path[1] == 0) return true;
    FILINFO fi;
    return f_stat(path, &fi) == FR_OK;
}

bool isDir(const char* path) {
    if (path[0] == '/' && path[1] == 0) return true;
    FILINFO fi;
    return f_stat(path, &fi) == FR_OK && (fi.fattrib & AM_DIR);
}

bool remove(const char* path) { return check(f_unlink(path)); }
bool rename(const char* from, const char* to) { return check(f_rename(from, to)); }

bool mkdir(const char* path) {
    const FRESULT r = f_mkdir(path);
    return r == FR_OK || r == FR_EXIST;
}

bool listDir(const char* path, std::vector<DirEntry>& out, size_t maxEntries) {
    DIR dir;
    if (f_opendir(&dir, path) != FR_OK) return false;
    FILINFO fi;
    while (out.size() < maxEntries && f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
        if (fi.fattrib & (AM_HID | AM_SYS)) continue;
        out.push_back(DirEntry{fi.fname, (fi.fattrib & AM_DIR) != 0, uint32_t(fi.fsize)});
    }
    f_closedir(&dir);
    return true;
}

}  // namespace fs
