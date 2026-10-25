// PicoCraft - a small voxel sandbox game for the ClockworkPi PicoCalc, on the PicoPods libraries.
//
// Core 0: game logic, keyboard, SD card, frame output (screen.cpp) and part of the ray casting.
// Core 1: the rest of the ray casting. The rows of a frame are handed out one
//         at a time (render_claimed), so core 0 can also feed the display and
//         read the slow keyboard while core 1 renders, and still take its share.
// Without a second core (browser build) core 0 renders everything.
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "gfx.h"
#include "pd/diag.h"
#include "pd/pd.h"
#include "render.h"
#include "screen.h"
#include "storage.h"
#include "textures.h"
#include "world.h"

namespace {

const char* const kStages[] = {"start", "display", "textures", "world", "ready"};
enum Stage { kStart, kDisplay, kTextures, kWorld, kReady };

uint16_t fbuf[2][RW * RH];
int back;                            // frame being drawn
const uint16_t* pending = nullptr;   // drawn, not sent to the display yet

// ------------------------------------------------------------------ keyboard
// Raw events: key state for movement, presses for commands. Letters are
// lower-cased so Shift (or Caps Lock) does not change the controls.
pd::input::Keyboard* kbd = nullptr;
bool key_down[256], key_hit[256];

void poll_keys() {
    pd::hal::RawKey ev;
    while (kbd->poll_raw(ev)) {
        const uint8_t k = pd::key::lower(ev.code);
        if (ev.state == pd::hal::KEY_RELEASED) {
            key_down[k] = false;
        } else {
            key_down[k] = true;
            if (ev.state == pd::hal::KEY_PRESSED) key_hit[k] = true;
        }
    }
}

bool down(uint8_t k) { return key_down[k]; }

bool hit(uint8_t k) {
    if (!key_hit[k]) return false;
    key_hit[k] = false;
    return true;
}

bool any_hit() {
    bool any = false;
    for (bool& h : key_hit) {
        any |= h;
        h = false;
    }
    return any;
}

void clear_keys() {
    memset(key_hit, 0, sizeof key_hit);
    memset(key_down, 0, sizeof key_down);
}

// ------------------------------------------------------------------ player
#define PW 0.3f      // half width
#define PH 1.8f      // height
#define EYE 1.62f

struct {
    float x, y, z, vy;
    float yaw, pitch;
    bool fly, ground, swim;
} pl;

int hotbar[9] = {B_GRASS, B_DIRT, B_STONE, B_COBBLE, B_LOG, B_PLANKS, B_GLASS, B_BRICK, B_LEAVES};
int slot;
int view_dist = VIEW_DIST_DEFAULT;
int save_slot;
bool show_debug;
char msg[40];
float msg_t;
float action_cd;

void say(const char* s) {
    strncpy(msg, s, sizeof msg - 1);
    msg[sizeof msg - 1] = 0;
    msg_t = 2.5f;
}

float wrapf(float v, float m) {
    while (v < 0) v += m;
    while (v >= m) v -= m;
    return v;
}

bool collides(float x, float y, float z) {
    int x0 = (int)floorf(x - PW), x1 = (int)floorf(x + PW);
    int y0 = (int)floorf(y), y1 = (int)floorf(y + PH - 0.001f);
    int z0 = (int)floorf(z - PW), z1 = (int)floorf(z + PW);
    for (int by = y0; by <= y1; by++)
        for (int bz = z0; bz <= z1; bz++)
            for (int bx = x0; bx <= x1; bx++)
                if (bsolid(wblock(bx, by, bz))) return true;
    return false;
}

void move_player(float dx, float dy, float dz) {
    // small steps, so the player never goes through a block
    int steps = (int)(fmaxf(fmaxf(fabsf(dx), fabsf(dy)), fabsf(dz)) / 0.2f) + 1;
    dx /= steps; dy /= steps; dz /= steps;
    pl.ground = false;
    for (int s = 0; s < steps; s++) {
        if (dx != 0) {
            if (!collides(pl.x + dx, pl.y, pl.z)) pl.x += dx;
            else if (dx > 0) pl.x = floorf(pl.x + PW + dx) - PW - 0.001f;
            else pl.x = floorf(pl.x - PW + dx) + 1 + PW + 0.001f;
            if (collides(pl.x, pl.y, pl.z)) pl.x -= dx;   // safety net
        }
        if (dz != 0) {
            if (!collides(pl.x, pl.y, pl.z + dz)) pl.z += dz;
            else if (dz > 0) pl.z = floorf(pl.z + PW + dz) - PW - 0.001f;
            else pl.z = floorf(pl.z - PW + dz) + 1 + PW + 0.001f;
            if (collides(pl.x, pl.y, pl.z)) pl.z -= dz;
        }
        if (dy != 0) {
            if (!collides(pl.x, pl.y + dy, pl.z)) pl.y += dy;
            else {
                if (dy < 0) { pl.y = floorf(pl.y + dy) + 1; pl.ground = true; }
                else pl.y = floorf(pl.y + PH + dy) - PH - 0.001f;
                if (collides(pl.x, pl.y, pl.z)) pl.y -= dy;
                pl.vy = 0;
                dy = 0;
            }
        }
    }
    pl.x = wrapf(pl.x, WX);
    pl.z = wrapf(pl.z, WZ);
    if (pl.y < -10) {   // fell out of the world (should not happen)
        pl.y = world_surface((int)pl.x, (int)pl.z) + 1;
        pl.vy = 0;
    }
}

void spawn() {
    // look for a dry column near the centre
    for (int r = 0; r < 60; r++) {
        int x = WX / 2 + (r % 11) - 5 + (r / 11) * 7, z = WZ / 2 + (r / 3) % 9;
        int y = world_surface(x, z);
        if (y > SEA_LEVEL + 1 && wblock(x, y - 1, z) != B_LEAVES) {
            pl.x = x + 0.5f; pl.y = y; pl.z = z + 0.5f;
            pl.vy = 0; pl.yaw = 0.6f; pl.pitch = -0.15f;
            return;
        }
    }
    pl.x = WX / 2 + 0.5f; pl.z = WZ / 2 + 0.5f; pl.y = WY - 2; pl.vy = 0;
}

// The time of a key press (or of boot) is the only randomness there is.
uint32_t new_seed() {
    const uint64_t t = pd::sys::now_us();
    uint32_t h = uint32_t(t) ^ (uint32_t(t >> 32) * 0x9E3779B9u);
    h ^= h >> 16; h *= 0x85EBCA6Bu; h ^= h >> 13; h *= 0xC2B2AE35u; h ^= h >> 16;
    return h;
}

void new_world(uint32_t seed) {
    world_generate(seed);
    spawn();
}

// ------------------------------------------------------------------ frames
bool dual_core;
std::atomic<uint32_t> job{0}, done{0};

void core1_main() {
    uint32_t seen = 0;
    for (;;) {
        uint32_t j;
        while ((j = job.load(std::memory_order_acquire)) == seen) pd::sys::sleep_us(20);
        seen = j;
        render_claimed(nullptr);
        done.store(j, std::memory_order_release);
    }
}

// Ray casts the world into fbuf[back]. Meanwhile the previous frame goes to
// the display and the keyboard is read.
void render_world(bool sel, int sx, int sy, int sz) {
    screen::finish();   // fbuf[back] may still be on its way to the display
    camera_t cam = {};
    cam.x = pl.x; cam.y = pl.y + EYE; cam.z = pl.z;
    cam.yaw = pl.yaw; cam.pitch = pl.pitch;
    cam.view_dist = view_dist;
    cam.sel = sel; cam.sx = sx; cam.sy = sy; cam.sz = sz;
    cam.fb = fbuf[back];
    cam.underwater = wblock((int)floorf(cam.x), (int)floorf(cam.y), (int)floorf(cam.z)) == B_WATER;
    render_setup(&cam);
    uint32_t j = 0;
    if (dual_core) {
        j = job.load(std::memory_order_relaxed) + 1;
        job.store(j, std::memory_order_release);
    }
    if (pending) {
        screen::start(pending);
        pending = nullptr;
    }
    poll_keys();   // ~16 ms per read on the PicoCalc: core 1 renders meanwhile
    render_claimed(screen::pump);
    if (dual_core)
        while (done.load(std::memory_order_acquire) != j) screen::pump();
}

// fbuf[back] is complete: it goes out during the next render_world() (or flush()).
void present() {
    pending = fbuf[back];
    back ^= 1;
}

// Puts the last presented frame on screen and waits for it.
void flush() {
    if (pending) {
        screen::start(pending);
        pending = nullptr;
    }
    screen::finish();
}

// The frame on screen, to draw a dialog over it.
uint16_t* shown_frame() {
    flush();
    return fbuf[back ^ 1];
}

void show(uint16_t* fb) {
    screen::start(fb);
    screen::finish();
}

void look_dir(float* dx, float* dy, float* dz) {
    float cp = cosf(pl.pitch);
    *dx = sinf(pl.yaw) * cp; *dy = sinf(pl.pitch); *dz = -cosf(pl.yaw) * cp;
}

// ------------------------------------------------------------------ HUD
void draw_hud(uint16_t* fb, float fps) {
    gfx_crosshair(fb);
    // hotbar
    int bw = 16, x0 = (RW - 9 * bw) / 2, y0 = RH - bw - 2;
    gfx_darken(fb, x0 - 1, y0 - 1, 9 * bw + 2, bw + 2);
    for (int i = 0; i < 9; i++) {
        int x = x0 + i * bw;
        gfx_icon(fb, x + 2, y0 + 2, hotbar[i], 12);
        if (i == slot) {
            gfx_frame(fb, x, y0, bw, bw, C_WHITE);
            gfx_frame(fb, x + 1, y0 + 1, bw - 2, bw - 2, C_WHITE);
        }
    }
    if (msg_t > 0) gfx_text_c(fb, RW / 2, y0 - 9, msg, C_WHITE, 1, 1);
    if (pl.fly) gfx_text(fb, RW - 22, 2, "FLY", C_YELLOW, 1, 1);
    if (show_debug) {
        char b[48];
        snprintf(b, sizeof b, "%d FPS %s", (int)(fps + 0.5f), dual_core ? "2 CORES" : "1 CORE");
        gfx_text(fb, 2, 2, b, C_WHITE, 1, 1);
        snprintf(b, sizeof b, "X%d Y%d Z%d", (int)pl.x, (int)pl.y, (int)pl.z);
        gfx_text(fb, 2, 9, b, C_WHITE, 1, 1);
        snprintf(b, sizeof b, "SEED %lu", (unsigned long)world_seed);
        gfx_text(fb, 2, 16, b, C_WHITE, 1, 1);
    }
}

void draw_help(uint16_t* fb) {
    gfx_darken(fb, 0, 0, RW, RH);
    gfx_text_c(fb, RW / 2, 6, "CONTROLS", C_YELLOW, 2, 1);
    static const char* l[] = {
        "WASD    MOVE",
        "ARROWS  LOOK AROUND",
        "SPACE   JUMP / SWIM / UP",
        "C       DOWN (FLYING)",
        "J / DEL BREAK BLOCK",
        "K / ENT PLACE BLOCK",
        "1-9 Q E PICK SLOT",
        "TAB     CHANGE BLOCK",
        "F       FLY ON/OFF",
        "F3      DEBUG INFO",
        "ESC     MENU",
        "",
        "PRESS A KEY",
    };
    for (unsigned i = 0; i < sizeof l / sizeof l[0]; i++)
        gfx_text(fb, 14, 26 + i * 9, l[i], i == 12 ? C_YELLOW : C_WHITE, 1, 1);
}

// ------------------------------------------------------------------ saves
void busy_screen(const char* s) {
    uint16_t* fb = shown_frame();
    gfx_darken(fb, 0, RH / 2 - 10, RW, 20);
    gfx_text_c(fb, RW / 2, RH / 2 - 3, s, C_WHITE, 1, 1);
    show(fb);
}

const char* save_error(int r) {
    switch (r) {
        case SAVE_NO_CARD: return "NO SD CARD";
        case SAVE_BAD_FILE: return "NOT A PICOCRAFT SAVE";
        default: return "SD CARD ERROR";
    }
}

void do_save() {
    busy_screen("SAVING...");
    save_hdr_t h;
    memset(&h, 0, sizeof h);
    memcpy(h.magic, "PCW1", 4);
    h.seed = world_seed;
    h.x = pl.x; h.y = pl.y; h.z = pl.z; h.yaw = pl.yaw; h.pitch = pl.pitch;
    h.fly = pl.fly; h.slot = uint8_t(slot); h.view_dist = uint8_t(view_dist);
    for (int i = 0; i < 9; i++) h.hotbar[i] = uint8_t(hotbar[i]);
    const int r = storage_save(save_slot, &h);
    char b[40];
    if (r) snprintf(b, sizeof b, "%s", save_error(r));
    else snprintf(b, sizeof b, "SAVED TO SLOT %d", save_slot + 1);
    say(b);
}

bool do_load(int s) {
    busy_screen("LOADING...");
    save_hdr_t h;
    const uint32_t old_seed = world_seed;
    const int r = storage_load(s, &h);
    char b[40];
    if (r) {
        if (r == SAVE_IO_ERROR) new_world(old_seed);   // the world may be half read
        if (r == SAVE_EMPTY) snprintf(b, sizeof b, "SLOT %d IS EMPTY", s + 1);
        else snprintf(b, sizeof b, "%s", save_error(r));
        say(b);
        return false;
    }
    world_seed = h.seed;
    pl.x = h.x; pl.y = h.y; pl.z = h.z; pl.yaw = h.yaw; pl.pitch = h.pitch; pl.vy = 0;
    pl.fly = h.fly;
    slot = h.slot % 9;
    if (h.view_dist >= VIEW_DIST_MIN && h.view_dist <= VIEW_DIST_MAX) view_dist = h.view_dist;
    for (int i = 0; i < 9; i++)
        if (h.hotbar[i] > 0 && h.hotbar[i] < B_COUNT) hotbar[i] = h.hotbar[i];
    snprintf(b, sizeof b, "LOADED SLOT %d", s + 1);
    say(b);
    return true;
}

// ------------------------------------------------------------------ menu
enum { M_RESUME, M_SAVE, M_LOAD, M_NEW, M_VIEW, M_FLY, M_HELP, M_COUNT };

void menu() {
    int sel = 0;
    bool help = false;
    clear_keys();
    for (;;) {
        if (help) {
            if (any_hit()) help = false;
        } else {
            if (hit(pd::key::Esc)) break;
            if (hit(pd::key::Up)) sel = (sel + M_COUNT - 1) % M_COUNT;
            if (hit(pd::key::Down)) sel = (sel + 1) % M_COUNT;
            int lr = hit(pd::key::Right) ? 1 : (hit(pd::key::Left) ? -1 : 0);
            if (lr && (sel == M_SAVE || sel == M_LOAD)) save_slot = (save_slot + SAVE_SLOTS + lr) % SAVE_SLOTS;
            if (lr && sel == M_VIEW) {
                view_dist += lr * 2;
                if (view_dist < VIEW_DIST_MIN) view_dist = VIEW_DIST_MIN;
                if (view_dist > VIEW_DIST_MAX) view_dist = VIEW_DIST_MAX;
            }
            if (hit(pd::key::Enter) || hit(' ')) {
                if (sel == M_RESUME) break;
                if (sel == M_SAVE) { do_save(); break; }
                if (sel == M_LOAD) { do_load(save_slot); break; }
                if (sel == M_NEW) {
                    busy_screen("CREATING WORLD...");
                    new_world(new_seed());
                    say("NEW WORLD");
                    break;
                }
                if (sel == M_VIEW) view_dist = VIEW_DIST_DEFAULT;
                if (sel == M_FLY) { pl.fly = !pl.fly; pl.vy = 0; }
                if (sel == M_HELP) { help = true; any_hit(); }
            }
        }

        render_world(false, 0, 0, 0);   // live background, darkened
        uint16_t* fb = fbuf[back];
        gfx_darken(fb, 0, 0, RW, RH);
        if (help) draw_help(fb);
        else {
            gfx_text_c(fb, RW / 2, 14, "PICOCRAFT", C_YELLOW, 3, 1);
            char items[M_COUNT][32];
            snprintf(items[M_RESUME], 32, "RESUME");
            snprintf(items[M_SAVE], 32, "SAVE     < SLOT %d >", save_slot + 1);
            snprintf(items[M_LOAD], 32, "LOAD     < SLOT %d >", save_slot + 1);
            snprintf(items[M_NEW], 32, "NEW WORLD");
            snprintf(items[M_VIEW], 32, "DISTANCE < %d >", view_dist);
            snprintf(items[M_FLY], 32, "FLYING: %s", pl.fly ? "ON" : "OFF");
            snprintf(items[M_HELP], 32, "CONTROLS");
            for (int i = 0; i < M_COUNT; i++) {
                int y = 46 + i * 14;
                if (i == sel) gfx_fill(fb, 16, y - 3, RW - 32, 11, 0x2945);
                gfx_text(fb, 24, y, items[i], i == sel ? C_YELLOW : C_WHITE, 1, 1);
            }
            gfx_text_c(fb, RW / 2, RH - 10, "ARROWS + ENTER   ESC BACK", C_GRAY, 1, 0);
        }
        present();
    }
    clear_keys();
}

// ------------------------------------------------------------------ title
void title_screen() {
    float t = 0;
    uint64_t last = pd::sys::now_us();
    clear_keys();
    float base_yaw = pl.yaw;
    for (;;) {
        if (hit(pd::key::Enter) || hit(' ')) break;
        if (hit('l')) {
            if (do_load(0)) break;
        }
        if (hit('n')) {
            busy_screen("CREATING WORLD...");
            new_world(new_seed());
            base_yaw = pl.yaw;
        }
        uint64_t now = pd::sys::now_us();
        t += (now - last) * 1e-6f;
        last = now;
        pl.yaw = base_yaw + t * 0.25f;   // slow turn around the spawn point
        render_world(false, 0, 0, 0);
        uint16_t* fb = fbuf[back];
        gfx_text_c(fb, RW / 2, 22, "PICOCRAFT", C_YELLOW, 3, 1);
        gfx_text_c(fb, RW / 2, 44, "A BLOCK WORLD ON THE PICOCALC", C_WHITE, 1, 1);
        if (((int)(t * 2)) & 1) gfx_text_c(fb, RW / 2, 100, "ENTER TO PLAY", C_WHITE, 1, 1);
        gfx_text_c(fb, RW / 2, 118, "N NEW WORLD   L LOAD SLOT 1", C_WHITE, 1, 1);
        if (msg_t > 0) { gfx_text_c(fb, RW / 2, 132, msg, C_YELLOW, 1, 1); msg_t -= 0.05f; }
        gfx_text_c(fb, RW / 2, RH - 9, "ESC IN GAME = MENU", C_GRAY, 1, 1);
        present();
    }
    clear_keys();
}

}  // namespace

// ------------------------------------------------------------------ main
int main() {
    pd::diag::begin("PicoCraft", PD_APP_VERSION, kStages, pd::array_count(kStages));
    pd::diag::stage(kStart);

    pd::TerminalConfig cfg;
    cfg.clock_khz = SYS_CLOCK_KHZ;
    cfg.audio = false;
    pd::Terminal& term = pd::terminal_init(cfg);
    pd::gfx::init(term.display());
    pd::diag::display_ready();
    pd::diag::stage(kDisplay);
    kbd = &term.keyboard();
    screen::init(term.display());

    // Colour bars (red, green, blue, white): a quick check of the panel.
    pd::diag::set_screen_log(false);
    pd::app::splash(pd::ui::theme(0), "PicoCraft", "A block world on the PicoCalc", true);
    const uint32_t splash_end = pd::sys::now_ms() + 1500;

    pd::diag::stage(kTextures);
    textures_init();
    pd::diag::stage(kWorld);
    new_world(new_seed());
    dual_core = pd::sys::launch_core1(core1_main);
    pd::diag::stage(kReady);
    pd::diag::done();
    while (!pd::sys::reached(splash_end)) pd::sys::sleep_ms(10);

    title_screen();

    uint64_t last = pd::sys::now_us();
    float fps = 0;
    for (;;) {
        uint64_t now = pd::sys::now_us();
        float dt = (now - last) * 1e-6f;
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        if (dt > 0) fps = fps * 0.9f + 0.1f / dt;

        // --- one-shot commands
        if (hit(pd::key::Esc)) {
            menu();
            last = pd::sys::now_us();
            continue;
        }
        if (hit(pd::key::F1)) {
            uint16_t* fb = shown_frame();
            draw_help(fb);
            show(fb);
            clear_keys();
            for (;;) {
                poll_keys();
                if (any_hit()) break;
                pd::sys::sleep_ms(10);
            }
            clear_keys();
            last = pd::sys::now_us();
            continue;
        }
        if (hit(pd::key::F3)) show_debug = !show_debug;
        if (hit('f')) { pl.fly = !pl.fly; pl.vy = 0; say(pl.fly ? "FLYING ON" : "FLYING OFF"); }
        for (int i = 0; i < 9; i++)
            if (hit(uint8_t('1' + i))) { slot = i; say(block_name(hotbar[slot])); }
        if (hit('q')) { slot = (slot + 8) % 9; say(block_name(hotbar[slot])); }
        if (hit('e')) { slot = (slot + 1) % 9; say(block_name(hotbar[slot])); }
        if (hit(pd::key::Tab)) {
            int b = hotbar[slot];
            do { b = b % (B_COUNT - 1) + 1; } while (b == B_BEDROCK);
            hotbar[slot] = b;
            say(block_name(b));
        }

        // --- view
        const float rot = 2.2f * dt;
        if (down(pd::key::Left)) pl.yaw -= rot;
        if (down(pd::key::Right)) pl.yaw += rot;
        if (down(pd::key::Up)) pl.pitch += rot * 0.8f;
        if (down(pd::key::Down)) pl.pitch -= rot * 0.8f;
        if (pl.pitch > 1.5f) pl.pitch = 1.5f;
        if (pl.pitch < -1.5f) pl.pitch = -1.5f;
        pl.yaw = wrapf(pl.yaw, 6.2831853f);

        // --- movement
        float fx = sinf(pl.yaw), fz = -cosf(pl.yaw);
        float mx = 0, mz = 0;
        if (down('w')) { mx += fx; mz += fz; }
        if (down('s')) { mx -= fx; mz -= fz; }
        if (down('d')) { mx -= fz; mz += fx; }
        if (down('a')) { mx += fz; mz -= fx; }
        float len = sqrtf(mx * mx + mz * mz);
        pl.swim = wblock((int)floorf(pl.x), (int)floorf(pl.y + 0.4f), (int)floorf(pl.z)) == B_WATER;
        float speed = pl.fly ? 8.0f : (pl.swim ? 2.6f : 4.3f);
        if (len > 0) { mx = mx / len * speed * dt; mz = mz / len * speed * dt; }

        float my;
        if (pl.fly) {
            float v = 0;
            if (down(' ')) v += 6.0f;
            if (down('c')) v -= 6.0f;
            my = v * dt;
            pl.vy = 0;
        } else {
            if (pl.swim) {
                pl.vy -= 6.0f * dt;
                if (pl.vy < -2.5f) pl.vy = -2.5f;
                if (down(' ')) pl.vy = 3.0f;
            } else {
                pl.vy -= 24.0f * dt;
                if (pl.vy < -30) pl.vy = -30;
                if (pl.ground && down(' ')) pl.vy = 7.6f;
            }
            my = pl.vy * dt;
        }
        move_player(mx, my, mz);

        // --- targeted block
        float ex = pl.x, ey = pl.y + EYE, ez = pl.z, dx, dy, dz;
        look_dir(&dx, &dy, &dz);
        int hx = 0, hy = 0, hz = 0, nx = 0, ny = 0, nz = 0;
        bool sel = world_pick(ex, ey, ez, dx, dy, dz, 5.0f, &hx, &hy, &hz, &nx, &ny, &nz);

        action_cd -= dt;
        bool brk = hit('j') | hit(pd::key::Del) | hit(pd::key::Backspace);
        bool plc = hit('k') | hit(pd::key::Enter);
        if (sel && action_cd <= 0) {
            if (brk && wblock(hx, hy, hz) != B_BEDROCK) {
                wset(hx, hy, hz, B_AIR);
                action_cd = 0.15f;
                sel = world_pick(ex, ey, ez, dx, dy, dz, 5.0f, &hx, &hy, &hz, &nx, &ny, &nz);
            } else if (plc && ny >= 0 && ny < WY) {
                int old = wblock(nx, ny, nz);
                wset(nx, ny, nz, hotbar[slot]);
                if (bsolid(hotbar[slot]) && collides(pl.x, pl.y, pl.z)) wset(nx, ny, nz, old);
                else action_cd = 0.15f;
            }
        }
        if (msg_t > 0) msg_t -= dt;

        // --- draw (the keys read meanwhile are handled next frame)
        render_world(sel, hx, hy, hz);
        draw_hud(fbuf[back], fps);
        present();
    }
}
