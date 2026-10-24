// POSIX implementation of fs:: for host-side tests.
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cerrno>
#include <cstring>

#include "../src/editor/storage.h"

namespace fs {

File::File() {}
File::~File() { close(); }

int g_failReadOpens = 0;  // test hook

bool File::open(const char* path, Mode mode) {
    if (mode == Mode::Read && g_failReadOpens > 0) { --g_failReadOpens; errno = EIO; return false; }
    close();
    const char* m = mode == Mode::Read ? "rb" : mode == Mode::ReadWrite ? "w+b" : "wb";
    FILE* f = fopen(path, m);
    if (!f) return false;
    impl_ = f;
    open_ = true;
    return true;
}
void File::close() {
    if (open_) fclose(static_cast<FILE*>(impl_));
    open_ = false;
    impl_ = nullptr;
}
bool File::seek(uint32_t pos) { return fseek(static_cast<FILE*>(impl_), long(pos), SEEK_SET) == 0; }
int File::read(void* buf, uint32_t n) {
    FILE* f = static_cast<FILE*>(impl_);
    size_t r = fread(buf, 1, n, f);
    if (r < n && ferror(f)) return -1;
    return int(r);
}
int File::write(const void* buf, uint32_t n) {
    return int(fwrite(buf, 1, n, static_cast<FILE*>(impl_)));
}
uint32_t File::size() {
    FILE* f = static_cast<FILE*>(impl_);
    long cur = ftell(f);
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fseek(f, cur, SEEK_SET);
    return uint32_t(s);
}
bool File::sync() { return fflush(static_cast<FILE*>(impl_)) == 0; }

bool mount() { return true; }
int lastError() { return errno; }
const char* lastErrorText() { return strerror(errno); }
bool exists(const char* p) { struct stat st; return stat(p, &st) == 0; }
bool isDir(const char* p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
bool remove(const char* p) { return ::remove(p) == 0; }
bool rename(const char* a, const char* b) { return ::rename(a, b) == 0; }
bool mkdir(const char* p) { return ::mkdir(p, 0755) == 0 || isDir(p); }
bool listDir(const char* path, std::vector<DirEntry>& out, size_t maxEntries) {
    DIR* d = opendir(path);
    if (!d) return false;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        if (out.size() >= maxEntries) break;
        std::string full = std::string(path) + "/" + n;
        struct stat st;
        stat(full.c_str(), &st);
        out.push_back(DirEntry{n, S_ISDIR(st.st_mode), uint32_t(st.st_size)});
    }
    closedir(d);
    return true;
}

}  // namespace fs
