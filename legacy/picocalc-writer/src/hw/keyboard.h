// PicoCalc keyboard (STM32 "south bridge" on I2C1, address 0x1F).
#pragma once
#include <cstdint>

namespace key {
// Key codes reported by the PicoCalc keyboard firmware.
constexpr uint8_t Backspace = 0x08, Tab = 0x09, Enter = 0x0A;
constexpr uint8_t Esc = 0xB1;
constexpr uint8_t Left = 0xB4, Up = 0xB5, Down = 0xB6, Right = 0xB7;
constexpr uint8_t Break = 0xD0, Insert = 0xD1, Home = 0xD2, Del = 0xD4, End = 0xD5;
constexpr uint8_t PageUp = 0xD6, PageDown = 0xD7;
constexpr uint8_t F1 = 0x81, F2 = 0x82, F3 = 0x83, F4 = 0x84, F5 = 0x85;
constexpr uint8_t F6 = 0x86, F7 = 0x87, F8 = 0x88, F9 = 0x89, F10 = 0x90;
// Modifiers (tracked internally, never returned as events)
constexpr uint8_t Alt = 0xA1, ShiftL = 0xA2, ShiftR = 0xA3, Sym = 0xA4, Ctrl = 0xA5;
constexpr uint8_t CapsLock = 0xC1;
}  // namespace key

struct KeyEvent {
    uint8_t code = 0;
    bool ctrl = false, shift = false, alt = false;
    // Printable byte to insert, or 0.
    char ch() const {
        if (ctrl || alt) return 0;
        if (code == key::Enter) return '\n';
        if (code >= 0x20 && code < 0x7F) return char(code);
        return 0;
    }
};

class Keyboard {
public:
    bool init();
    // Non-blocking. Handles modifier tracking and auto-repeat.
    bool poll(KeyEvent& ev);
    // Blocking wait for a key.
    KeyEvent wait();
    // Battery percentage (0-100) or -1 if unavailable. Bit 7 = charging.
    int battery();

private:
    bool readFifo(uint8_t& state, uint8_t& code);
    bool ctrl_ = false, shiftL_ = false, shiftR_ = false, alt_ = false;
    uint8_t held_ = 0;          // last pressed non-modifier key (for repeat)
    uint32_t nextRepeat_ = 0;   // ms timestamp
    uint32_t lastPoll_ = 0;
};
