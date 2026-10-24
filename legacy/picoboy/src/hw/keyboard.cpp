// PicoBoy - PicoCalc keyboard driver
// Protocol (same as ClockworkPi's reference code): write register 0x09, then
// read two bytes: [state, key]. Zero means the event FIFO is empty.
#include "keyboard.h"
#include "../config.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/sync.h"

static constexpr int QSIZE = 64;
static KeyEvent queue[QSIZE];
static volatile uint32_t q_head = 0, q_tail = 0;   // head: core1 writes, tail: core0 reads

void kbd_init() {
    i2c_init(KBD_I2C, KBD_I2C_HZ);
    gpio_set_function(KBD_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(KBD_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(KBD_PIN_SDA);
    gpio_pull_up(KBD_PIN_SCL);
}

static bool read_event(uint8_t* state, uint8_t* key) {
    uint8_t reg = 0x09;
    uint8_t buf[2] = {0, 0};
    if (i2c_write_timeout_us(KBD_I2C, KBD_ADDR, &reg, 1, false, 50000) != 1) return false;
    sleep_us(KBD_READ_GAP_US);
    if (i2c_read_timeout_us(KBD_I2C, KBD_ADDR, buf, 2, false, 50000) != 2) return false;
    *state = buf[0];
    *key = buf[1];
    return buf[0] != 0 || buf[1] != 0;
}

void kbd_poll() {
    uint8_t state, key;
    for (int i = 0; i < 8; i++) {
        if (!read_event(&state, &key)) return;
        if (state < KEY_PRESSED || state > KEY_RELEASED) continue;
        if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
        uint32_t next = (q_head + 1) % QSIZE;
        if (next == q_tail) return;   // queue full: drop
        queue[q_head] = KeyEvent{key, state};
        __dmb();
        q_head = next;
    }
}

bool kbd_get(KeyEvent* ev) {
    if (q_tail == q_head) return false;
    __dmb();
    *ev = queue[q_tail];
    __dmb();
    q_tail = (q_tail + 1) % QSIZE;
    return true;
}
