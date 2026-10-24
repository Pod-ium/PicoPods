// PicoCalc Writer - distraction-free text editor firmware for the
// ClockworkPi PicoCalc with a Raspberry Pi Pico 2 W.
#include "diag.h"
#include "hw/keyboard.h"
#include "hw/lcd.h"
#include "pico/stdlib.h"
#include "ui/app.h"

int main() {
    diag::begin();
    diag::stage(diag::kStart);
    stdio_init_all();  // UART debug output on GP0/GP1
    diag::stage(diag::kDisplay);
    lcd::init();
    diag::displayReady();
    diag::stage(diag::kKeyboard);
    static Keyboard kbd;
    kbd.init();
    static App app(kbd);
    app.run();
}
