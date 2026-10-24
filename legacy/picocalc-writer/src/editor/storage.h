// Thin file-system abstraction.
// On the device it wraps FatFs (storage_fatfs.cpp); in host tests it wraps
// stdio/POSIX (test/storage_host.cpp). The editor core only uses this API.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace fs {

enum class Mode {
    Read,        // existing file, read only
    ReadWrite,   // create/truncate, read + write (used for the add buffer)
    Write        // create/truncate, write only
};

class File {
public:
    File();
    ~File();
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    bool open(const char* path, Mode mode);
    void close();
    bool isOpen() const { return open_; }
    bool seek(uint32_t pos);
    // Returns bytes transferred or -1 on error.
    int read(void* buf, uint32_t n);
    int write(const void* buf, uint32_t n);
    uint32_t size();
    bool sync();

private:
    bool open_ = false;
    void* impl_ = nullptr;  // FIL* or FILE*
};

struct DirEntry {
    std::string name;
    bool isDir;
    uint32_t size;
};

// Code of the last failed operation (FatFs FRESULT on the device, errno on
// the host), for error messages. 0 if none.
int lastError();
// Short human-readable text for lastError().
const char* lastErrorText();

bool mount();
bool exists(const char* path);
bool isDir(const char* path);
bool remove(const char* path);
bool rename(const char* from, const char* to);
bool mkdir(const char* path);
// Lists a directory (no "." / ".."); returns false on error.
bool listDir(const char* path, std::vector<DirEntry>& out, size_t maxEntries);

}  // namespace fs
