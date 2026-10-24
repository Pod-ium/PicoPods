// PicoBoy - PicoCalc keyboard (STM32 over I2C)
// kbd_poll() runs on core 1 and fills a queue; core 0 consumes with kbd_get().
#pragma once
#include <stdint.h>

enum KeyState : uint8_t { KEY_PRESSED = 1, KEY_HOLD = 2, KEY_RELEASED = 3 };

struct KeyEvent {
    uint8_t key;     // key code (letters normalised to lowercase)
    uint8_t state;   // KeyState
};

void kbd_init();            // call on the core that will poll
void kbd_poll();            // reads all pending events (blocking I2C), core 1
bool kbd_get(KeyEvent* ev); // non-blocking, core 0
