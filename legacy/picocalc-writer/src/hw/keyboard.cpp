#include "keyboard.h"

#include "board.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

namespace {
constexpr uint8_t kAddr = 0x1F;
constexpr uint8_t kRegFifo = 0x09;
constexpr uint8_t kRegBattery = 0x0B;
constexpr uint32_t kRepeatDelayMs = 450;
constexpr uint32_t kRepeatRateMs = 45;
constexpr uint32_t kPollIntervalMs = 4;

uint32_t now_ms() { return to_ms_since_boot(get_absolute_time()); }

bool repeatable(uint8_t code, bool ctrl) {
    if (ctrl) return false;
    if (code == key::Esc) return false;
    if (code >= key::F1 && code <= key::F10) return false;
    return true;
}
}  // namespace

bool Keyboard::init() {
    i2c_init(KBD_I2C, KBD_I2C_HZ);
    gpio_set_function(KBD_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(KBD_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(KBD_SDA_PIN);
    gpio_pull_up(KBD_SCL_PIN);
    // Drain anything queued during boot.
    uint8_t s, c;
    for (int i = 0; i < 32 && readFifo(s, c) && c; ++i) {}
    return true;
}

bool Keyboard::readFifo(uint8_t& state, uint8_t& code) {
    uint8_t reg = kRegFifo;
    if (i2c_write_timeout_us(KBD_I2C, kAddr, &reg, 1, false, 20000) != 1) return false;
    sleep_us(KBD_READ_DELAY_US);
    uint8_t buf[2] = {0, 0};
    if (i2c_read_timeout_us(KBD_I2C, kAddr, buf, 2, false, 20000) != 2) return false;
    state = buf[0];
    code = buf[1];
    return true;
}

int Keyboard::battery() {
    uint8_t reg = kRegBattery;
    if (i2c_write_timeout_us(KBD_I2C, kAddr, &reg, 1, false, 20000) != 1) return -1;
    sleep_us(KBD_READ_DELAY_US);
    uint8_t buf[2] = {0, 0};
    if (i2c_read_timeout_us(KBD_I2C, kAddr, buf, 2, false, 20000) != 2) return -1;
    return buf[1];
}

bool Keyboard::poll(KeyEvent& ev) {
    const uint32_t t = now_ms();
    if (t - lastPoll_ < kPollIntervalMs) return false;
    lastPoll_ = t;

    for (int guard = 0; guard < 8; ++guard) {
        uint8_t state = 0, code = 0;
        if (!readFifo(state, code) || code == 0) break;
        const bool down = (state == 1);
        const bool up = (state == 3);
        switch (code) {
            case key::Ctrl:   if (down) ctrl_ = true;   if (up) ctrl_ = false;   continue;
            case key::ShiftL: if (down) shiftL_ = true; if (up) shiftL_ = false; continue;
            case key::ShiftR: if (down) shiftR_ = true; if (up) shiftR_ = false; continue;
            case key::Alt:    if (down) alt_ = true;    if (up) alt_ = false;    continue;
            case key::Sym:
            case key::CapsLock: continue;
            default: break;
        }
        if (up) {
            if (code == held_) held_ = 0;
            continue;
        }
        if (!down) continue;  // "hold" notifications: we do our own repeat
        ev.code = code;
        ev.ctrl = ctrl_;
        ev.alt = alt_;
        ev.shift = shiftL_ || shiftR_;
        if (ev.ctrl && code >= 'A' && code <= 'Z') ev.code = uint8_t(code - 'A' + 'a');
        held_ = repeatable(code, ctrl_) ? code : 0;
        nextRepeat_ = t + kRepeatDelayMs;
        return true;
    }

    if (held_ && int32_t(t - nextRepeat_) >= 0) {
        ev.code = held_;
        ev.ctrl = ctrl_;
        ev.alt = alt_;
        ev.shift = shiftL_ || shiftR_;
        nextRepeat_ = t + kRepeatRateMs;
        return true;
    }
    return false;
}

KeyEvent Keyboard::wait() {
    KeyEvent ev;
    while (!poll(ev)) sleep_ms(1);
    return ev;
}
