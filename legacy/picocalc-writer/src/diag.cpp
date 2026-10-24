// Boot diagnostics: on-screen start-up log, a breadcrumb that survives a
// reset (watchdog scratch 3, not used by the SDK or by the UF2 Loader), and a
// crash screen for HardFaults / panics instead of a silent lock-up.
#include "diag.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "hardware/structs/powman.h"
#include "hardware/structs/watchdog.h"
#include "hw/lcd.h"
#include "pico/stdlib.h"
#include "ui/gfx.h"

namespace {
constexpr uint32_t kMagic = 0xF3D10000u;
constexpr uint32_t kMagicMask = 0xFFFF0000u;
constexpr uint16_t kDone = 0xFFFF;

int g_y = 0;
bool g_lcdReady = false;
const char* g_prevStage = nullptr;
int g_prevId = -1;
uint32_t g_resetBits = 0;

const char* const kStageNames[] = {
    "start",          "display",        "keyboard",       "SD card",
    "work folder",    "scratch files",  "settings",       "last file",
    "first screen",
};

void rawText(int x, int y, const char* s, uint16_t fg, uint16_t bg) {
    Cell cells[53];
    int n = 0;
    while (s[n] && n < 53) {
        cells[n] = {uint16_t(uint8_t(s[n])), fg, bg};
        ++n;
    }
    gfx::drawCells(x, y, LCD_W_MAX - x, *kFonts[0], cells, n, bg);
}

void line(const char* s, uint16_t fg) {
    if (!g_lcdReady) return;
    if (g_y > LCD_H_MAX - 12) return;
    rawText(4, g_y, s, fg, 0x0000);
    g_y += 13;
}
}  // namespace

namespace diag {

void begin() {
    const uint32_t v = watchdog_hw->scratch[3];
    if ((v & kMagicMask) == kMagic) {
        const uint16_t st = uint16_t(v & 0xFFFF);
        if (st != kDone && st < sizeof(kStageNames) / sizeof(kStageNames[0])) {
            g_prevStage = kStageNames[st];
            g_prevId = st;
        }
    }
    g_resetBits = powman_hw->chip_reset;
}

void displayReady() {
    g_lcdReady = true;
    g_y = 4;
    line("PicoCalc Writer " FW_VERSION, lcd::rgb(255, 255, 255));
    char buf[64];
    snprintf(buf, sizeof buf, "reset reason 0x%08lx", (unsigned long)g_resetBits);
    line(buf, lcd::rgb(140, 140, 140));
    if (g_prevStage) {
        snprintf(buf, sizeof buf, "! previous start stopped at: %s", g_prevStage);
        line(buf, lcd::rgb(255, 180, 0));
    }
}

void stage(Stage s) {
    watchdog_hw->scratch[3] = kMagic | uint32_t(s);
    char buf[64];
    snprintf(buf, sizeof buf, "%s...", kStageNames[s]);
    line(buf, lcd::rgb(120, 200, 120));
}

void note(const char* s) { line(s, lcd::rgb(200, 200, 200)); }

void done() { watchdog_hw->scratch[3] = kMagic | kDone; }

bool hadIncompleteStart() { return g_prevStage != nullptr; }
int previousStage() { return g_prevId; }

[[noreturn]] void crashScreen(const char* what, uint32_t pc, uint32_t lr, const char* msg) {
    const uint16_t bg = lcd::rgb(120, 0, 0), fg = lcd::rgb(255, 255, 255);
    lcd::fillRect(0, 0, LCD_W_MAX, LCD_H_MAX, bg);
    rawText(8, 8, "PicoCalc Writer crashed", fg, bg);
    rawText(8, 24, what, fg, bg);
    char buf[64];
    snprintf(buf, sizeof buf, "pc=%08lx lr=%08lx", (unsigned long)pc, (unsigned long)lr);
    rawText(8, 40, buf, fg, bg);
    if (msg) rawText(8, 56, msg, fg, bg);
    const uint32_t v = watchdog_hw->scratch[3];
    if ((v & kMagicMask) == kMagic && (v & 0xFFFF) < sizeof(kStageNames) / sizeof(kStageNames[0])) {
        snprintf(buf, sizeof buf, "during: %s", kStageNames[v & 0xFFFF]);
        rawText(8, 72, buf, fg, bg);
    }
    rawText(8, 100, "Please send a photo of this screen.", fg, bg);
    rawText(8, 116, "Power-cycle to restart.", fg, bg);
    for (;;) tight_loop_contents();
}

}  // namespace diag

// ---- fault hooks -------------------------------------------------------------
extern "C" {

void fw_hardfault_c(uint32_t* frame) {
    // Exception frame: r0 r1 r2 r3 r12 lr pc xpsr
    diag::crashScreen("HardFault", frame[6], frame[5], nullptr);
}

// Overrides the SDK's weak default handler.
__attribute__((naked)) void isr_hardfault(void) {
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b fw_hardfault_c\n");
}

// Installed through PICO_PANIC_FUNCTION (see CMakeLists.txt).
void __attribute__((noreturn)) fw_panic(const char* fmt, ...) {
    char msg[64] = "";
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(msg, sizeof msg, fmt, ap);
        va_end(ap);
    }
    diag::crashScreen("panic", 0, uint32_t(uintptr_t(__builtin_return_address(0))), msg);
}
}
