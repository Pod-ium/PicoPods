// picocalc_ui - menus, dialogs and file browser (layout follows PicoCalc Writer)
#include "pcui/ui.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
extern "C" {
#include "ff.h"
}

namespace pcui {

const Theme THEME_NIGHT = {
    0x121214, 0xD6D0C4, 0x6E6C68,   // bg, fg, dim
    0x222228, 0xD6D0C4,             // panel
    0xE8A040, 0x121214,             // highlight
    0xE8A040, 0xE57373,             // accent, error
};

namespace {

constexpr int kPanelX = 8, kPanelW = 304;
constexpr int kInnerX = 16, kInnerW = 288;

const Font& F() { return gfx::font(gfx::FONT_8x16); }
int inner_cells() { return kInnerW / F().w; }

// Copies at most `cells` characters of s into out, padded with spaces to `cells`.
void pad(char* out, size_t cap, const char* s, int cells) {
    size_t o = 0;
    int c = 0;
    for (const char* p = s; *p && o + 1 < cap; ++p) {
        if ((uint8_t(*p) & 0xC0) != 0x80) {
            if (c == cells) break;
            ++c;
        }
        out[o++] = *p;
    }
    while (c < cells && o + 1 < cap) { out[o++] = ' '; ++c; }
    out[o] = 0;
}

void panel(const Theme& t, int y, int h, const char* title) {
    gfx::fill(kPanelX, y, kPanelW, h, t.panel_bg);
    gfx::frame(kPanelX, y, kPanelW, h, t.accent);
    if (title) gfx::text(kInnerX, y + 4, kInnerW, F(), title, t.accent, t.panel_bg);
}

void line(const Theme& t, int y, const char* s, bool hi, bool dim = false, Color color = 0xFFFFFFFF) {
    char buf[96];
    pad(buf, sizeof buf, s, inner_cells());
    Color fg = hi ? t.hi_fg : dim ? t.dim : color != 0xFFFFFFFF ? color : t.panel_fg;
    gfx::text(kInnerX, y, kInnerW, F(), buf, fg, hi ? t.hi_bg : t.panel_bg);
}

// "label ........ right" in exactly inner_cells() characters
void compose(char* out, size_t cap, const char* label, const char* right) {
    const int rc = right ? gfx::text_cells(right) + 1 : 0;   // +1: keep a gap
    char left[96];
    pad(left, sizeof left, label, inner_cells() - rc);
    snprintf(out, cap, "%s%s%s", left, right ? " " : "", right ? right : "");
}

void item_text(const MenuItem& it, char* out, size_t cap) {
    char right[48] = {0};
    if (it.kind == MenuItem::CHOICE && it.value && it.count > 0) {
        int v = *it.value;
        if (v < 0 || v >= it.count) v = 0;
        snprintf(right, sizeof right, "< %s >", it.options[v]);
    } else if (it.kind == MenuItem::RANGE && it.value) {
        char num[24];
        snprintf(num, sizeof num, it.fmt ? it.fmt : "%d", *it.value);
        snprintf(right, sizeof right, "< %s >", num);
    } else if (it.hint) {
        snprintf(right, sizeof right, "%s", it.hint);
    }
    compose(out, cap, it.label, right[0] ? right : nullptr);
}

bool adjust(const MenuItem& it, int dir) {
    if (!it.value) return false;
    int v = *it.value;
    if (it.kind == MenuItem::CHOICE && it.count > 0) {
        v = ((v + dir) % it.count + it.count) % it.count;
    } else if (it.kind == MenuItem::RANGE) {
        v += dir * it.step;
        if (v < it.min) v = it.min;
        if (v > it.max) v = it.max;
    } else {
        return false;
    }
    if (v == *it.value) return false;
    *it.value = v;
    return true;
}

}  // namespace

// ---------------------------------------------------------------- menu
MenuItem action(const char* label, const char* hint) {
    MenuItem m;
    m.kind = MenuItem::ACTION;
    m.label = label;
    m.hint = hint;
    return m;
}

MenuItem choice(const char* label, int* value, const char* const* options, int count) {
    MenuItem m;
    m.kind = MenuItem::CHOICE;
    m.label = label;
    m.value = value;
    m.options = options;
    m.count = count;
    return m;
}

MenuItem range(const char* label, int* value, int min, int max, int step, const char* fmt) {
    MenuItem m;
    m.kind = MenuItem::RANGE;
    m.label = label;
    m.value = value;
    m.min = min;
    m.max = max;
    m.step = step;
    m.fmt = fmt;
    return m;
}

int menu(Input& in, const Theme& t, const char* title, const MenuItem* items, int n, const MenuOptions& opt) {
    if (n <= 0) return -1;
    const int visible = n < 15 ? n : 15;
    const int footer_h = opt.footer ? F().h : 0;
    const int h = (visible + 1) * F().h + 12 + footer_h;
    const int y0 = (SCREEN_H - h) / 2;
    panel(t, y0, h, title);
    if (opt.footer) line(t, y0 + 8 + (visible + 1) * F().h, opt.footer, false, true);

    int sel = opt.initial < 0 ? 0 : opt.initial >= n ? n - 1 : opt.initial;
    for (int g = 0; g < n && !items[sel].enabled; ++g) sel = (sel + 1) % n;
    int top = 0;
    for (;;) {
        if (sel < top) top = sel;
        if (sel >= top + visible) top = sel - visible + 1;
        for (int i = 0; i < visible; ++i) {
            const MenuItem& it = items[top + i];
            char s[96];
            item_text(it, s, sizeof s);
            line(t, y0 + 8 + (i + 1) * F().h, s, top + i == sel, !it.enabled);
        }
        const int k = in.wait_key();
        const MenuItem& cur = items[sel];
        if (k == key::Up || k == key::Down) {
            const int d = k == key::Up ? -1 : 1;
            int s = sel;
            for (int g = 0; g < n; ++g) {
                s = (s + d + n) % n;
                if (items[s].enabled) break;
            }
            sel = s;
        } else if (k == key::Left || k == key::Right) {
            if (cur.enabled && adjust(cur, k == key::Right ? 1 : -1) && opt.on_change) opt.on_change(sel, opt.user);
        } else if (k == key::Enter) {
            if (!cur.enabled) continue;
            if (cur.kind == MenuItem::ACTION) return sel;
            bool changed;
            if (cur.kind == MenuItem::RANGE && cur.value && *cur.value >= cur.max) {
                changed = *cur.value != cur.min;
                *cur.value = cur.min;   // wrap around
            } else {
                changed = adjust(cur, 1);
            }
            if (changed && opt.on_change) opt.on_change(sel, opt.user);
        } else if (k == key::Esc || k == key::Backspace) {
            return -1;
        }
    }
}

// ---------------------------------------------------------------- dialogs
void message(Input& in, const Theme& t, const char* title, const char* const* lines, int n, Color color) {
    if (n > 8) n = 8;
    const int h = (n + 3) * F().h + 12;
    const int y0 = (SCREEN_H - h) / 2;
    panel(t, y0, h, title);
    for (int i = 0; i < n; ++i) line(t, y0 + 8 + (i + 1) * F().h, lines[i], false, false, color);
    line(t, y0 + 8 + (n + 2) * F().h, "Press any key", false, true);
    in.wait_key();
}

bool confirm(Input& in, const Theme& t, const char* title, const char* question) {
    const int h = 4 * F().h + 12;
    const int y0 = (SCREEN_H - h) / 2;
    panel(t, y0, h, title);
    line(t, y0 + 8 + F().h, question, false);
    line(t, y0 + 8 + 3 * F().h, "Enter: yes    Esc: no", false, true);
    for (;;) {
        const int k = in.wait_key();
        if (k == key::Enter || k == 'y' || k == 'Y') return true;
        if (k == key::Esc || k == key::Backspace || k == 'n' || k == 'N') return false;
    }
}

void progress(const Theme& t, const char* title, const char* label, uint32_t done, uint32_t total) {
    const int h = 4 * F().h + 12;
    const int y0 = (SCREEN_H - h) / 2;
    if (done == 0) panel(t, y0, h, title);
    line(t, y0 + 8 + F().h, label, false);
    const int bx = kInnerX, by = y0 + 8 + 3 * F().h, bw = kInnerW, bh = 10;
    const int filled = total ? int(uint64_t(done) * uint64_t(bw - 4) / total) : 0;
    if (done == 0) {
        gfx::frame(bx, by, bw, bh, t.dim);
        gfx::fill(bx + 1, by + 1, bw - 2, bh - 2, t.panel_bg);
    }
    if (filled > 0) gfx::fill(bx + 2, by + 2, filled, bh - 4, t.accent);
}

// ---------------------------------------------------------------- browser
namespace {

struct Entry {
    char name[112];
    bool dir;
    uint32_t size;
};
constexpr int kMaxEntries = 300;
Entry g_entries[kMaxEntries];
int g_count;

int cmp_entry(const void* a, const void* b) {
    const Entry* x = (const Entry*)a;
    const Entry* y = (const Entry*)b;
    const bool xu = strcmp(x->name, "..") == 0, yu = strcmp(y->name, "..") == 0;
    if (xu != yu) return xu ? -1 : 1;
    if (x->dir != y->dir) return x->dir ? -1 : 1;
    return strcasecmp(x->name, y->name);
}

bool scan(const char* dir, FileFilter filter) {
    static DIR d;
    static FILINFO fi;
    g_count = 0;
    if (strcmp(dir, "/") != 0) {
        strcpy(g_entries[0].name, "..");
        g_entries[0].dir = true;
        g_entries[0].size = 0;
        g_count = 1;
    }
    if (f_opendir(&d, dir) != FR_OK) return false;
    while (g_count < kMaxEntries) {
        if (f_readdir(&d, &fi) != FR_OK || fi.fname[0] == 0) break;
        if (fi.fname[0] == '.' || (fi.fattrib & (AM_HID | AM_SYS))) continue;
        const bool is_dir = fi.fattrib & AM_DIR;
        if (!is_dir && filter && !filter(fi.fname)) continue;
        if (strlen(fi.fname) >= sizeof(g_entries[0].name)) continue;
        Entry& e = g_entries[g_count++];
        strcpy(e.name, fi.fname);
        e.dir = is_dir;
        e.size = (uint32_t)fi.fsize;
    }
    f_closedir(&d);
    qsort(g_entries, g_count, sizeof(Entry), cmp_entry);
    return true;
}

void human_size(uint32_t n, char* out, size_t cap) {
    if (n < 1024) snprintf(out, cap, "%uB", unsigned(n));
    else if (n < 1024 * 1024) snprintf(out, cap, "%uK", unsigned((n + 512) / 1024));
    else snprintf(out, cap, "%u.%uM", unsigned(n >> 20), unsigned(((n & 0xFFFFF) * 10) >> 20));
}

void join(char* out, size_t cap, const char* dir, const char* name) {
    if (strcmp(dir, "/") == 0) snprintf(out, cap, "/%s", name);
    else snprintf(out, cap, "%s/%s", dir, name);
}

// Moves dir to its parent; returns the name of the folder we left.
void go_up(char* dir, char* left_name, size_t cap) {
    char* slash = strrchr(dir, '/');
    if (!slash) { strcpy(dir, "/"); left_name[0] = 0; return; }
    snprintf(left_name, cap, "%s", slash + 1);
    if (slash == dir) dir[1] = 0;
    else *slash = 0;
}

}  // namespace

bool browse(Input& in, const Theme& t, const char* title, char* dir, size_t dir_cap, char* out, size_t out_cap,
            const BrowseOptions& opt) {
    const int rows = (SCREEN_H - 8) / F().h - 2;
    char want[112] = {0};
    if (opt.select) snprintf(want, sizeof want, "%s", opt.select);

    for (;;) {
        if (!scan(dir, opt.filter)) {
            if (strcmp(dir, "/") == 0) {
                const char* lines[] = {"Cannot read the SD card."};
                message(in, t, "Error", lines, 1, t.error);
                return false;
            }
            strcpy(dir, "/");
            continue;
        }

        gfx::fill(0, 0, SCREEN_W, SCREEN_H, t.panel_bg);
        gfx::frame(4, 2, SCREEN_W - 8, SCREEN_H - 4, t.accent);
        {
            char head[160];
            snprintf(head, sizeof head, "%s %s", title, dir);
            const int cells = inner_cells();
            const int len = gfx::text_cells(head);
            const char* shown = head;
            char clipped[160];
            if (len > cells) {   // keep the end of long paths visible
                snprintf(clipped, sizeof clipped, "..%s", head + (strlen(head) - (cells - 2)));
                shown = clipped;
            }
            gfx::text(kInnerX, 6, kInnerW, F(), shown, t.accent, t.panel_bg);
        }
        gfx::text(kInnerX, SCREEN_H - F().h - 6, kInnerW, F(),
                  opt.allow_cancel ? "Enter open  Bksp up  Esc cancel" : "Enter open  Bksp up", t.dim, t.panel_bg);

        const int n = g_count;
        int sel = 0, top = 0;
        if (want[0]) {
            for (int i = 0; i < n; ++i)
                if (strcmp(g_entries[i].name, want) == 0) { sel = i; break; }
            want[0] = 0;
        }
        bool redraw_all = true;
        int drawn_sel = -1, drawn_top = -1;

        auto draw_row = [&](int i) {
            const int idx = top + i;
            char s[128] = {0};
            if (idx < n) {
                const Entry& e = g_entries[idx];
                char right[16] = {0};
                if (e.dir) snprintf(right, sizeof right, "%s", strcmp(e.name, "..") ? "<dir>" : "");
                else if (opt.show_size) human_size(e.size, right, sizeof right);
                char label[120];
                snprintf(label, sizeof label, "%s%s", e.name, e.dir && strcmp(e.name, "..") ? "/" : "");
                compose(s, sizeof s, label, right[0] ? right : nullptr);
            } else if (idx == 0) {
                snprintf(s, sizeof s, "%s", opt.empty_hint ? opt.empty_hint : "(empty folder)");
            }
            const bool hi = idx == sel && idx < n;
            char buf[128];
            pad(buf, sizeof buf, s, inner_cells());
            gfx::text(kInnerX, 6 + (i + 1) * F().h + 2, kInnerW, F(), buf,
                      hi ? t.hi_fg : (idx < n ? t.panel_fg : t.dim), hi ? t.hi_bg : t.panel_bg);
        };

        for (;;) {
            if (sel < top) top = sel;
            if (sel >= top + rows) top = sel - rows + 1;
            if (redraw_all || top != drawn_top) {
                for (int i = 0; i < rows; ++i) draw_row(i);
                redraw_all = false;
            } else if (sel != drawn_sel) {
                draw_row(drawn_sel - top);
                draw_row(sel - top);
            }
            drawn_sel = sel;
            drawn_top = top;

            const int k = in.wait_key();
            if (k == key::Esc && opt.allow_cancel) return false;
            if (k == key::Backspace || k == key::Left) {
                if (strcmp(dir, "/") != 0) { go_up(dir, want, sizeof want); break; }
                continue;
            }
            if (n == 0) continue;
            if (k == key::Up) sel = (sel + n - 1) % n;
            else if (k == key::Down) sel = (sel + 1) % n;
            else if (k == key::PageUp) sel = sel > rows ? sel - rows : 0;
            else if (k == key::PageDown) sel = sel + rows < n ? sel + rows : n - 1;
            else if (k == key::Enter || k == key::Right) {
                const Entry& e = g_entries[sel];
                if (strcmp(e.name, "..") == 0) { go_up(dir, want, sizeof want); break; }
                if (e.dir) {
                    char nd[256];
                    join(nd, sizeof nd, dir, e.name);
                    snprintf(dir, dir_cap, "%s", nd);
                    break;
                }
                if (k == key::Enter) {
                    join(out, out_cap, dir, e.name);
                    return true;
                }
            }
        }
    }
}

}  // namespace pcui
