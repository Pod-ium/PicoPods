// PicoBoy - Game Boy emulator firmware for the ClockworkPi PicoCalc (Raspberry Pi Pico 2 W)
//
// Core 0: emulation, video streaming (DMA IRQ), audio, SD card, UI.
// Core 1: keyboard polling (the keyboard controller needs slow, blocking I2C reads).
#include "config.h"
#include "gb/gb.h"
#include "hw/video.h"
#include "hw/audio.h"
#include "hw/keyboard.h"
#include "hw/sdcard.h"
#include "hw/romstore.h"
#include "pcui/lcd.h"
#include "pcui/gfx.h"
#include "pcui/ui.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/flash.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include <string.h>
#include <stdio.h>
#include <strings.h>
extern "C" {
#include "ff.h"
}

static FATFS fs;
static gb::GameBoy gbx;
static uint8_t framebuf[2][gb::SCREEN_W * gb::SCREEN_H];
static uint8_t cart_ram[CART_RAM_MAX];

static char cur_dir[256] = ROM_DIR;
static char rom_path[256];
static char sav_path[260];

struct Palette { const char* name; uint32_t rgb[4]; };
static const Palette PALETTES[] = {
    {"DMG green", {0xE0F8D0, 0x88C070, 0x346856, 0x081820}},
    {"Pocket",    {0xC4CFA1, 0x8B956D, 0x4D533C, 0x1F1F1F}},
    {"Grey",      {0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000}},
    {"Amber",     {0xFFE9B0, 0xE0A040, 0x8A4A10, 0x2A1004}},
};
static constexpr int NUM_PALETTES = sizeof(PALETTES) / sizeof(PALETTES[0]);
static int palette_idx = 0;
static int volume_setting = DEFAULT_VOLUME;

static const pcui::Theme& theme = pcui::THEME_NIGHT;
static const char* const PALETTE_NAMES[] = {"DMG green", "Pocket", "Grey", "Amber"};

// Keyboard events arrive from core 1 through a queue; this adds auto-repeat
// and hands them to the shared UI library.
class KbdInput : public pcui::Input {
public:
    int wait_key() override {
        for (;;) {
            KeyEvent ev;
            while (kbd_get(&ev)) {
                if (ev.state == KEY_PRESSED) {
                    held_ = ev.key;
                    next_ = make_timeout_time_ms(400);
                    return ev.key;
                }
                if (ev.state == KEY_RELEASED && ev.key == held_) held_ = -1;
            }
            if (held_ >= 0 && time_reached(next_)) {
                next_ = make_timeout_time_ms(90);
                return held_;
            }
            sleep_ms(2);
        }
    }
    void flush() {
        KeyEvent ev;
        while (kbd_get(&ev)) {}
        held_ = -1;
    }

private:
    int held_ = -1;
    absolute_time_t next_ = nil_time;
};
static KbdInput input;

static void show_message(const char* title, const char* l1, const char* l2 = nullptr, bool error = true) {
    const char* lines[2] = {l1, l2};
    input.flush();
    pcui::message(input, theme, title, lines, l2 ? 2 : 1, error ? theme.error : theme.panel_fg);
}

// ------------------------------------------------------------------ core 1
static void core1_main() {
    flash_safe_execute_core_init();   // lets core 0 pause us safely while it writes flash
    kbd_init();
    for (;;) {
        kbd_poll();
        sleep_ms(1);
    }
}

// ------------------------------------------------------------------ saves
static void make_sav_path() {
    snprintf(sav_path, sizeof sav_path, "%s", rom_path);
    char* dot = strrchr(sav_path, '.');
    char* slash = strrchr(sav_path, '/');
    if (dot && dot > slash) *dot = 0;
    strncat(sav_path, ".sav", sizeof sav_path - strlen(sav_path) - 1);
}

static void load_save() {
    const gb::CartInfo& ci = gbx.info();
    memset(cart_ram, 0xFF, sizeof cart_ram);
    if (!ci.battery || ci.ram_size == 0) return;
    static FIL f;
    if (f_open(&f, sav_path, FA_READ) != FR_OK) return;
    UINT got;
    f_read(&f, cart_ram, ci.ram_size, &got);
    f_close(&f);
}

static bool write_save() {
    const gb::CartInfo& ci = gbx.info();
    if (!ci.battery || ci.ram_size == 0) return true;
    static FIL f;
    if (f_open(&f, sav_path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    UINT put = 0;
    FRESULT r = f_write(&f, cart_ram, ci.ram_size, &put);
    f_close(&f);
    gbx.clear_ram_dirty();
    return r == FR_OK && put == ci.ram_size;
}

// ------------------------------------------------------------------ loading
static const char* g_loading_name = "";

static void progress_cb(uint32_t done, uint32_t total, void*) {
    static uint32_t last = 0;
    if (done < last) last = 0;
    if (done != total && done - last < 32768) return;
    last = done;
    pcui::progress(theme, "Loading", g_loading_name, done, total);
}

static bool load_rom(const char* path) {
    const char* name = strrchr(path, '/');
    g_loading_name = name ? name + 1 : path;
    pcui::progress(theme, "Loading", g_loading_name, 0, 1);

    uint32_t size = 0;
    const char* err = nullptr;
    const uint8_t* rom = romstore_load(path, &size, progress_cb, nullptr, &err);
    if (!rom) {
        show_message("Cannot load ROM", err ? err : "Unknown error");
        return false;
    }

    gb::LoadResult r = gbx.load(rom, size, cart_ram, sizeof cart_ram);
    switch (r) {
        case gb::LoadResult::OK: break;
        case gb::LoadResult::CGB_ONLY:
            show_message("Game Boy Color only", "This game needs a GBC.", "PicoBoy emulates the original DMG.");
            return false;
        case gb::LoadResult::UNSUPPORTED_MBC: {
            char m[40];
            snprintf(m, sizeof m, "Cartridge type 0x%02X", rom[0x147]);
            show_message("Unsupported cartridge", m);
            return false;
        }
        default:
            show_message("Cannot load ROM", "Not a valid Game Boy ROM");
            return false;
    }
    snprintf(rom_path, sizeof rom_path, "%s", path);
    make_sav_path();
    load_save();
    gbx.reset();   // cart RAM contents are preserved; reset clears only the console
    return true;
}

// ------------------------------------------------------------------ game screen
static void draw_game_chrome() {
    const pcui::Font& f = pcui::gfx::font(pcui::gfx::FONT_6x12);
    pcui::gfx::fill(0, 0, pcui::SCREEN_W, GAME_Y, 0x000000);
    pcui::gfx::fill(0, GAME_Y + GAME_H, pcui::SCREEN_W, pcui::SCREEN_H - GAME_Y - GAME_H, 0x000000);
    const char* title = gbx.info().title[0] ? gbx.info().title : "PicoBoy";
    pcui::gfx::text(4, 2, 0, f, title, 0x606870, 0x000000);
    pcui::gfx::text(pcui::SCREEN_W - 4 - 9 * f.w, 2, 0, f, "Esc: menu", 0x606870, 0x000000);
}

enum class PauseAction { RESUME, RESET, CHANGE_ROM };

static void on_pause_change(int, void*) { audio_set_volume(volume_setting); }

// Human-readable name of a PicoCalc key code, for the Controls screen
static const char* key_name(int k) {
    static char buf[4][8];
    static int slot = 0;
    switch (k) {
        case KEY_ENTER: return "Enter";
        case KEY_ESC: return "Esc";
        case KEY_BACKSPACE: return "Bksp";
        case ' ': return "Space";
        case 0x09: return "Tab";
        default: break;
    }
    char* b = buf[slot++ & 3];
    if (k >= 0x81 && k <= 0x89) snprintf(b, 8, "F%d", k - 0x80);
    else if (k == 0x90) snprintf(b, 8, "F10");
    else if (k >= 'a' && k <= 'z') snprintf(b, 8, "%c", k - 'a' + 'A');
    else if (k > 32 && k < 127) snprintf(b, 8, "%c", k);
    else snprintf(b, 8, "0x%02X", k);
    return b;
}

// Lists the current key bindings (taken from config.h, so it stays accurate if you remap)
static void show_controls() {
    static char lines[6][40];
    snprintf(lines[0], 40, "D-pad     Arrow keys");
    snprintf(lines[1], 40, "A         %s or %s", key_name(BIND_A1), key_name(BIND_A2));
    snprintf(lines[2], 40, "B         %s or %s", key_name(BIND_B1), key_name(BIND_B2));
    snprintf(lines[3], 40, "Start     %s", key_name(BIND_START));
    snprintf(lines[4], 40, "Select    %s or %s", key_name(BIND_SELECT1), key_name(BIND_SELECT2));
    snprintf(lines[5], 40, "Menu      %s or %s", key_name(BIND_PAUSE1), key_name(BIND_PAUSE2));
    const char* ptrs[6] = {lines[0], lines[1], lines[2], lines[3], lines[4], lines[5]};
    input.flush();
    pcui::message(input, theme, "Controls", ptrs, 6);
}

// bg: the game frame to restore behind the menu after a larger dialog was shown
static PauseAction pause_menu(const uint8_t* bg) {
    const bool saved_ok = write_save();
    const char* footer = !gbx.info().battery ? nullptr : saved_ok ? "Game saved" : "Save FAILED";
    pcui::MenuItem items[] = {
        pcui::action("Resume", "Esc"),
        pcui::action("Reset game"),
        pcui::action("Load another ROM"),
        pcui::action("Controls"),
        pcui::choice("Palette", &palette_idx, PALETTE_NAMES, NUM_PALETTES),
        pcui::range("Volume", &volume_setting, 0, 100, 10, "%d%%"),
    };
    pcui::MenuOptions opt;
    opt.footer = footer;
    opt.on_change = on_pause_change;
    input.flush();
    for (;;) {
        int picked = pcui::menu(input, theme, "PAUSED", items, opt);
        switch (picked) {
            case -1: case 0: return PauseAction::RESUME;
            case 1: return PauseAction::RESET;
            case 2: return PauseAction::CHANGE_ROM;
            case 3:
                show_controls();
                if (bg) {   // repaint the game image, then the menu reopens on top of it
                    video_invalidate();
                    video_submit(bg);
                    video_wait_idle();
                }
                opt.initial = 3;
                break;
            default: break;
        }
    }
}

static uint8_t map_key(int key) {
    switch (key) {
        case KEY_UP: return gb::BTN_UP;
        case KEY_DOWN: return gb::BTN_DOWN;
        case KEY_LEFT: return gb::BTN_LEFT;
        case KEY_RIGHT: return gb::BTN_RIGHT;
        case BIND_A1: case BIND_A2: return gb::BTN_A;
        case BIND_B1: case BIND_B2: return gb::BTN_B;
        case BIND_START: return gb::BTN_START;
        case BIND_SELECT1: case BIND_SELECT2: return gb::BTN_SELECT;
        default: return 0;
    }
}

// "60/11 fps" in the title bar: emulated frames and screen updates per second
static void draw_stats(uint32_t emu, uint32_t shown, bool audio_ok) {
    const pcui::Font& f = pcui::gfx::font(pcui::gfx::FONT_6x12);
    char s[40];
    snprintf(s, sizeof s, "%u/%u fps%s", (unsigned)emu, (unsigned)shown, audio_ok ? "" : " no snd");
    pcui::gfx::text(112, 2, 24 * f.w, f, s, 0x606870, 0x000000);
}

// Runs the loaded game until the user chooses "Load another ROM".
static void run_game() {
    constexpr int AUDIO_TARGET = 1100;   // samples buffered ahead (~34 ms)
    int w = 0;                           // framebuffer being rendered
    int last_shown = -1;
    uint8_t buttons = 0;
    uint8_t latch[8] = {0};
    bool save_pending = false;
    int quiet_frames = 0;
    constexpr int64_t FRAME_US = 16742;   // 70224 cycles at 4.194304 MHz
    bool audio_ok = true;
    absolute_time_t next_frame = get_absolute_time();
    absolute_time_t next_stats = make_timeout_time_ms(1000);
    uint32_t emu_frames = 0, shown_frames = 0, stats_emu = 0, stats_shown = 0;

    video_set_palette(PALETTES[palette_idx].rgb);
    memset(framebuf, 0, sizeof framebuf);
    pcui::gfx::fill(0, 0, pcui::SCREEN_W, pcui::SCREEN_H, 0x000000);
    draw_game_chrome();
    input.flush();
    audio_silence();

    for (;;) {
        // ---- input
        bool pause = false;
        KeyEvent ev;
        while (kbd_get(&ev)) {
            if (ev.state == KEY_PRESSED && (ev.key == BIND_PAUSE1 || ev.key == BIND_PAUSE2)) { pause = true; continue; }
            uint8_t b = map_key(ev.key);
            if (!b) continue;
            if (ev.state == KEY_RELEASED) {
                buttons &= ~b;
            } else {
                buttons |= b;
                // A quick tap arrives as press+release together: keep it visible to the
                // game for a few frames, since games only read the buttons once per frame.
                for (int i = 0; i < 8; i++) if (b & (1 << i)) latch[i] = 4;
            }
        }
        if (pause) {
            video_wait_idle();
            audio_silence();
            PauseAction a = pause_menu(last_shown >= 0 ? framebuf[last_shown] : nullptr);
            save_pending = false;
            if (a == PauseAction::CHANGE_ROM) { audio_silence(); return; }
            if (a == PauseAction::RESET) { gbx.reset(); memset(framebuf, 0, sizeof framebuf); }
            video_set_palette(PALETTES[palette_idx].rgb);
            draw_game_chrome();
            buttons = 0;
            input.flush();
            audio_silence();
            memset(latch, 0, sizeof latch);
            video_invalidate();
            if (last_shown >= 0) video_submit(framebuf[last_shown]);
            next_frame = get_absolute_time();
            continue;
        }
        uint8_t effective = buttons;
        for (int i = 0; i < 8; i++) if (latch[i]) { effective |= 1 << i; latch[i]--; }
        gbx.set_buttons(effective);

        // ---- emulate one frame
        gbx.set_framebuffer(framebuf[w]);
        gbx.run_frame();
        emu_frames++;
        if (!video_busy()) {
            // Between two screen frames the LCD is free: refresh the status readout
            if (time_reached(next_stats)) {
                draw_stats(emu_frames - stats_emu, shown_frames - stats_shown, audio_ok);
                stats_emu = emu_frames;
                stats_shown = shown_frames;
                next_stats = make_timeout_time_ms(1000);
            }
            video_submit(framebuf[w]);
            last_shown = w;
            w ^= 1;
            shown_frames++;
        }

        // ---- audio + pacing
        // Normally the audio clock sets the speed. If the audio DMA ever stops
        // advancing, fall back to a 59.73 Hz timer instead of waiting forever.
        audio_push(gbx.apu().samples(), gbx.apu().sample_count());
        gbx.apu().clear_samples();
        if (audio_ok) {
            absolute_time_t give_up = make_timeout_time_ms(100);
            while (audio_buffered() > AUDIO_TARGET) {
                video_poll();
                if (time_reached(give_up)) { audio_ok = false; break; }
            }
            next_frame = make_timeout_time_us(FRAME_US);
        } else {
            while (!time_reached(next_frame)) video_poll();
            next_frame = delayed_by_us(next_frame, FRAME_US);
            if (absolute_time_diff_us(next_frame, get_absolute_time()) > 100000) next_frame = get_absolute_time();
        }

        // ---- autosave a few seconds after the game stops writing cart RAM
        if (gbx.ram_dirty()) { gbx.clear_ram_dirty(); save_pending = true; quiet_frames = 0; }
        else if (save_pending && ++quiet_frames >= AUTOSAVE_FRAMES) { write_save(); save_pending = false; }
    }
}

// ------------------------------------------------------------------ startup
static bool mount_sd() {
    for (;;) {
        if (f_mount(&fs, "", 1) == FR_OK) return true;
        pcui::gfx::fill(0, 0, pcui::SCREEN_W, pcui::SCREEN_H, theme.bg);
        show_message("No SD card",
                     sd_card_present() ? "Card not readable (FAT32/exFAT?)" : "Insert an SD card",
                     "then press any key");
    }
}

static bool is_rom(const char* name) {
    const char* dot = strrchr(name, '.');
    return dot && (strcasecmp(dot, ".gb") == 0 || strcasecmp(dot, ".gbc") == 0);
}

int main() {
    if (SYS_CLOCK_KHZ > 150000) {
        vreg_set_voltage(VREG_VOLTAGE_1_15);
        sleep_ms(10);
    }
    set_sys_clock_khz(SYS_CLOCK_KHZ, true);
    stdio_init_all();

    pcui::lcd::init();
    video_init();
    audio_init();
    audio_set_volume(volume_setting);
    sd_hw_init();
    romstore_init();
    multicore_launch_core1(core1_main);

    {   // splash, with colour bars that should read red, green, blue, white
        const pcui::Font& big = pcui::gfx::font(pcui::gfx::FONT_12x24);
        const pcui::Font& small = pcui::gfx::font(pcui::gfx::FONT_8x16);
        pcui::gfx::fill(0, 0, pcui::SCREEN_W, pcui::SCREEN_H, theme.bg);
        pcui::gfx::text((pcui::SCREEN_W - 7 * big.w) / 2, 120, 0, big, "PicoBoy", theme.accent, theme.bg);
        const char* sub = "Game Boy emulator";
        pcui::gfx::text((pcui::SCREEN_W - pcui::gfx::text_cells(sub) * small.w) / 2, 152, 0, small, sub, theme.dim, theme.bg);
        static const uint32_t bars[4] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF};
        for (int i = 0; i < 4; i++) pcui::gfx::fill(64 + i * 50, 190, 42, 20, bars[i]);
        sleep_ms(1500);
    }

    mount_sd();
    {   // start in ROM_DIR if it exists, else the root
        FILINFO fi;
        if (f_stat(ROM_DIR, &fi) != FR_OK || !(fi.fattrib & AM_DIR)) strcpy(cur_dir, "/");
    }

    char select_buf[112] = {0};
    for (;;) {
        char path[256];
        pcui::BrowseOptions opt;
        opt.filter = is_rom;
        opt.select = select_buf[0] ? select_buf : nullptr;
        opt.empty_hint = "No .gb files here";
        opt.allow_cancel = false;
        input.flush();
        if (!pcui::browse(input, theme, "PicoBoy", cur_dir, sizeof cur_dir, path, sizeof path, opt)) {
            mount_sd();
            continue;
        }
        const char* slash = strrchr(path, '/');
        snprintf(select_buf, sizeof select_buf, "%.111s", slash ? slash + 1 : path);
        if (!load_rom(path)) continue;
        run_game();
    }
}
