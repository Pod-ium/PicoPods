// PicoCalc Writer - distraction-free text editor firmware for the
// ClockworkPi PicoCalc with a Raspberry Pi Pico 2 W, built on picodev.
#include "app.h"
#include "diag.h"

int main() {
    pd::diag::begin("PicoCalc Writer", FW_VERSION, diag::stageNames(), diag::kStageCount);
    diag::stage(diag::kStart);
    pd::Terminal& term = pd::terminal_init();   // UART debug output on GP0/GP1
    diag::stage(diag::kDisplay);
    pd::gfx::init(term.display());
    pd::diag::display_ready();
    diag::stage(diag::kKeyboard);
    static App app(term.keyboard());
    app.run();
}
