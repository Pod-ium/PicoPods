// pd::sys and pd::board on a PC (host builds: simulator and unit tests).
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "pd/board.h"
#include "pd/sys.h"

namespace pd::sys {
namespace {
const auto g_start = std::chrono::steady_clock::now();
uint32_t g_scratch[8] = {};
}  // namespace

uint64_t now_us() {
    using namespace std::chrono;
    return uint64_t(duration_cast<microseconds>(steady_clock::now() - g_start).count());
}
uint32_t now_ms() { return uint32_t(now_us() / 1000); }
void sleep_ms(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
void sleep_us(uint64_t us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }

bool set_clock_khz(uint32_t) { return true; }
uint32_t clock_khz() { return 150000; }

bool launch_core1(void (*fn)()) {
    static bool launched = false;
    if (launched || !fn) return false;
    launched = true;
    std::thread(fn).detach();
    return true;
}

void reboot() {
    fprintf(stderr, "[host] reboot requested\n");
    exit(0);
}
void reboot_to_bootloader() {
    fprintf(stderr, "[host] reboot to bootloader requested\n");
    exit(0);
}

uint32_t scratch(int i) { return (i >= 0 && i < 8) ? g_scratch[i] : 0; }
void set_scratch(int i, uint32_t v) {
    if (i >= 0 && i < 8) g_scratch[i] = v;
}
uint32_t reset_reason() { return 0; }
uint32_t free_heap() { return 256u * 1024; }

}  // namespace pd::sys

namespace pd::board {
const char* name() { return "host"; }
const char* chip() { return "host"; }
bool has_wifi() { return false; }
uint32_t flash_size() { return 4u * 1024 * 1024; }
uint32_t ram_size() { return 520u * 1024; }
uint32_t default_clock_khz() { return 150000; }
uint32_t fast_clock_khz() { return 200000; }
}  // namespace pd::board
