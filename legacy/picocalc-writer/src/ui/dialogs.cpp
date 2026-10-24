#include "dialogs.h"

#include <algorithm>
#include <cstdio>
#include <strings.h>

#include "../editor/storage.h"
#include "../hw/lcd.h"

namespace dlg {
namespace {

constexpr int kPanelX = 8;
constexpr int kPanelW = 304;
constexpr int kInnerX = 16;
constexpr int kInnerW = 288;

const Font& F() { return *kFonts[1]; }  // 8x16 for dialogs
int innerCells() { return kInnerW / F().w; }

// Byte offset of the cell-th character in a UTF-8 string.
size_t byteOfCell(const std::string& s, int cell) {
    size_t i = 0;
    int c = 0;
    while (i < s.size()) {
        if ((uint8_t(s[i]) & 0xC0) != 0x80) {
            if (c == cell) return i;
            ++c;
        }
        ++i;
    }
    return s.size();
}

std::string clipCells(const std::string& s, int cells) {
    return s.substr(0, byteOfCell(s, cells));
}

std::string padRight(std::string s, int cells) {
    s = clipCells(s, cells);
    for (int n = gfx::textCells(s); n < cells; ++n) s += ' ';
    return s;
}

// Keep the end of a long path visible.
std::string clipLeft(const std::string& s, int cells) {
    const int n = gfx::textCells(s);
    if (n <= cells) return s;
    return ".." + s.substr(byteOfCell(s, n - cells + 2));
}

std::vector<std::string> wrap(const std::string& text, int cells) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        std::string para = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        while (gfx::textCells(para) > cells) {
            size_t cut = byteOfCell(para, cells);
            size_t sp = para.rfind(' ', cut);
            if (sp == std::string::npos || sp == 0) sp = cut;
            out.push_back(para.substr(0, sp));
            para = para.substr(sp < para.size() && para[sp] == ' ' ? sp + 1 : sp);
        }
        out.push_back(para);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return out;
}

void panel(Ui& ui, int y, int h, const char* title) {
    const Theme& t = ui.th();
    gfx::fill(kPanelX, y, kPanelW, h, t.panelBg);
    gfx::frame(kPanelX, y, kPanelW, h, t.accent);
    if (title) gfx::drawText(kInnerX, y + 4, kInnerW, F(), title, t.accent, t.panelBg);
}

void line(Ui& ui, int y, const std::string& s, bool hi = false) {
    const Theme& t = ui.th();
    gfx::drawText(kInnerX, y, kInnerW, F(), padRight(s, innerCells()),
                  hi ? t.panelHiFg : t.panelFg, hi ? t.panelHi : t.panelBg);
}

std::string humanSize(uint32_t n) {
    char b[16];
    if (n < 1024) snprintf(b, sizeof b, "%uB", unsigned(n));
    else if (n < 1024 * 1024) snprintf(b, sizeof b, "%uK", unsigned((n + 512) / 1024));
    else snprintf(b, sizeof b, "%.1fM", double(n) / (1024.0 * 1024.0));
    return b;
}

}  // namespace

std::string join(const std::string& dir, const std::string& name) {
    if (dir.empty() || dir == "/") return "/" + name;
    return dir + "/" + name;
}

std::string parent(const std::string& path) {
    const size_t p = path.rfind('/');
    if (p == std::string::npos || p == 0) return "/";
    return path.substr(0, p);
}

std::string baseName(const std::string& path) {
    const size_t p = path.rfind('/');
    return p == std::string::npos ? path : path.substr(p + 1);
}

namespace {
// picocalc_ui reads keys through this interface. Modifiers are dropped: the
// library only needs arrows, Enter, Esc and Backspace.
class KbdInput : public pcui::Input {
public:
    explicit KbdInput(Keyboard& k) : kbd_(k) {}
    int wait_key() override { return kbd_.wait().code; }

private:
    Keyboard& kbd_;
};
}  // namespace

pcui::Theme pcuiTheme(const Theme& t) {
    pcui::Theme p;
    p.bg = lcd::to888(t.bg);
    p.fg = lcd::to888(t.fg);
    p.dim = lcd::to888(t.dim);
    p.panel_bg = lcd::to888(t.panelBg);
    p.panel_fg = lcd::to888(t.panelFg);
    p.hi_bg = lcd::to888(t.panelHi);
    p.hi_fg = lcd::to888(t.panelHiFg);
    p.accent = lcd::to888(t.accent);
    p.error = 0xE57373;
    return p;
}

int menu(Ui& ui, const char* title, const std::vector<pcui::MenuItem>& items,
         const pcui::MenuOptions& opt) {
    KbdInput in(ui.kbd);
    return pcui::menu(in, pcuiTheme(ui.th()), title, items.data(), int(items.size()), opt);
}

bool prompt(Ui& ui, const char* title, std::string& value, const char* hint, bool mask) {
    const Theme& t = ui.th();
    const int h = 4 * F().h + 8;
    const int y0 = 100;
    panel(ui, y0, h, title);
    line(ui, y0 + 4 + 3 * F().h, hint ? hint : "Enter = OK   Esc = cancel");
    std::string s = value;
    size_t cur = s.size();
    int scroll = 0;
    const int cells = innerCells();
    bool reveal = false;
    for (;;) {
        const int curCell = gfx::textCells(s.substr(0, cur));
        if (curCell < scroll) scroll = curCell;
        if (curCell - scroll >= cells - 1) scroll = curCell - cells + 2;
        const std::string shown = (mask && !reveal) ? std::string(size_t(gfx::textCells(s)), '*') : s;
        const std::string vis = shown.substr(byteOfCell(shown, scroll));
        gfx::drawText(kInnerX, y0 + 4 + int(1.5 * F().h), kInnerW, F(), padRight(vis, cells), t.fg,
                      t.bg, curCell - scroll, t.cursor);
        const KeyEvent ev = ui.kbd.wait();
        if (ev.code == key::Enter) { value = s; return true; }
        if (ev.code == key::Esc) return false;
        if (mask && ev.code == key::Tab) {
            reveal = !reveal;
            continue;
        }
        if (ev.code == key::Left || (ev.code == key::Backspace)) {
            if (cur == 0) continue;
            size_t p = cur - 1;
            while (p > 0 && (uint8_t(s[p]) & 0xC0) == 0x80) --p;
            if (ev.code == key::Backspace) s.erase(p, cur - p);
            cur = p;
        } else if (ev.code == key::Right || ev.code == key::Del) {
            if (cur >= s.size()) continue;
            size_t p = cur + 1;
            while (p < s.size() && (uint8_t(s[p]) & 0xC0) == 0x80) ++p;
            if (ev.code == key::Del) s.erase(cur, p - cur);
            else cur = p;
        } else if (ev.code == key::Home || ev.code == key::Up) {
            cur = 0;
        } else if (ev.code == key::End || ev.code == key::Down) {
            cur = s.size();
        } else if (ev.ctrl && ev.code == 'u') {
            s.clear();
            cur = 0;
        } else if (char c = ev.ch()) {
            if (c == '\n') continue;
            if (s.size() < 255) s.insert(cur++, 1, c);
        }
    }
}

int confirm(Ui& ui, const char* title, const std::string& text, bool allowCancel) {
    std::vector<std::string> lines = wrap(text, innerCells());
    const int h = (int(lines.size()) + 3) * F().h + 8;
    const int y0 = (LCD_H_MAX - h) / 2;
    panel(ui, y0, h, title);
    for (size_t i = 0; i < lines.size(); ++i) line(ui, y0 + 4 + int(i + 1) * F().h, lines[i]);
    line(ui, y0 + 4 + int(lines.size() + 2) * F().h,
         allowCancel ? "[Y]es  [N]o  [Esc] cancel" : "[Y]es  [N]o");
    for (;;) {
        const KeyEvent ev = ui.kbd.wait();
        if (ev.code == 'y' || ev.code == 'Y' || ev.code == key::Enter) return 1;
        if (ev.code == 'n' || ev.code == 'N') return 0;
        if (ev.code == key::Esc) {
            if (allowCancel) return -1;
            return 0;
        }
    }
}

void message(Ui& ui, const char* title, const std::vector<std::string>& in) {
    std::vector<std::string> lines;
    for (const std::string& s : in)
        for (const std::string& w : wrap(s, innerCells())) lines.push_back(w);
    if (lines.size() > 15) lines.resize(15);
    const int h = (int(lines.size()) + 3) * F().h + 8;
    const int y0 = (LCD_H_MAX - h) / 2;
    panel(ui, y0, h, title);
    for (size_t i = 0; i < lines.size(); ++i) line(ui, y0 + 4 + int(i + 1) * F().h, lines[i]);
    line(ui, y0 + 4 + int(lines.size() + 2) * F().h, "Press any key");
    ui.kbd.wait();
}

bool browse(Ui& ui, std::string& cwd, std::string& outPath, const char* title) {
    const Theme& t = ui.th();
    const int rows = (LCD_H_MAX - 8) / F().h - 2;
    for (;;) {
        if (!fs::isDir(cwd.c_str())) cwd = "/";
        std::vector<fs::DirEntry> entries;
        if (!fs::listDir(cwd.c_str(), entries, 512)) {
            message(ui, "Error", {"Cannot read folder " + cwd});
            return false;
        }
        entries.erase(std::remove_if(entries.begin(), entries.end(),
                                     [](const fs::DirEntry& e) { return e.name == ".fwedit"; }),
                      entries.end());
        std::sort(entries.begin(), entries.end(), [](const fs::DirEntry& a, const fs::DirEntry& b) {
            if (a.isDir != b.isDir) return a.isDir;
            return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
        });
        if (cwd != "/") entries.insert(entries.begin(), fs::DirEntry{"..", true, 0});

        gfx::fill(0, 0, LCD_W_MAX, LCD_H_MAX, t.panelBg);
        gfx::frame(4, 2, LCD_W_MAX - 8, LCD_H_MAX - 4, t.accent);
        gfx::drawText(kInnerX, 6, kInnerW, F(), clipLeft(std::string(title) + " " + cwd, innerCells()),
                      t.accent, t.panelBg);
        gfx::drawText(kInnerX, LCD_H_MAX - F().h - 6, kInnerW, F(),
                      padRight("Enter open  Bksp up  Esc cancel", innerCells()), t.dim, t.panelBg);
        const int n = int(entries.size());
        int sel = 0, top = 0;
        bool reload = false;
        while (!reload) {
            if (sel < top) top = sel;
            if (sel >= top + rows) top = sel - rows + 1;
            for (int i = 0; i < rows; ++i) {
                const int idx = top + i;
                std::string s;
                if (idx < n) {
                    const fs::DirEntry& e = entries[idx];
                    const std::string right = e.isDir ? "<dir>" : humanSize(e.size);
                    const int avail = innerCells() - int(right.size()) - 1;
                    s = padRight(e.name + (e.isDir && e.name != ".." ? "/" : ""), avail) + " " + right;
                } else if (idx == 0) {
                    s = "(empty folder)";
                }
                const bool hi = idx == sel && idx < n;
                gfx::drawText(kInnerX, 6 + (i + 1) * F().h + 2, kInnerW, F(), padRight(s, innerCells()),
                              hi ? t.panelHiFg : t.panelFg, hi ? t.panelHi : t.panelBg);
            }
            const KeyEvent ev = ui.kbd.wait();
            if (ev.code == key::Esc) return false;
            if (n == 0 && ev.code != key::Backspace && ev.code != key::Left) continue;
            if (ev.code == key::Up) sel = (sel + n - 1) % n;
            else if (ev.code == key::Down) sel = (sel + 1) % n;
            else if (ev.code == key::PageUp) sel = std::max(0, sel - rows);
            else if (ev.code == key::PageDown) sel = std::min(n - 1, sel + rows);
            else if (ev.code == key::Backspace || ev.code == key::Left) {
                if (cwd != "/") { cwd = parent(cwd); reload = true; }
            } else if (ev.code == key::Enter || ev.code == key::Right) {
                const fs::DirEntry& e = entries[sel];
                if (e.name == "..") { cwd = parent(cwd); reload = true; }
                else if (e.isDir) { cwd = join(cwd, e.name); reload = true; }
                else if (ev.code == key::Enter) { outPath = join(cwd, e.name); return true; }
            }
        }
    }
}

void help(Ui& ui) {
    static const char* const kLines[] = {
        "PicoCalc Writer - keyboard",
        "",
        "Esc              Menu",
        "Ctrl+N  Ctrl+O   New / Open",
        "Ctrl+S           Save",
        "Ctrl+Shift+S     Save as",
        "Ctrl+F  Ctrl+G   Find / Find next (also F3)",
        "Ctrl+R           Replace all",
        "Ctrl+L           Go to line",
        "Ctrl+Z  Ctrl+Y   Undo / Redo",
        "Ctrl+A           Select all",
        "Ctrl+C/X/V       Copy / Cut / Paste",
        "Shift+arrows     Select text",
        "Ctrl+Left/Right  Previous / next word",
        "Home  End        Row start / end (Alt+L/R)",
        "PgUp  PgDn       Page up / down  (Alt+U/D)",
        "Ctrl+Up/Down     Start / end of document",
        "Ins              Insert / overwrite",
        "Tab              Indent (4 spaces)",
        "F1 help  F2 font size  F4 status bar",
        "F5 theme  F6 syntax colours  Ctrl+T stats",
        "F7 export ePub  F8 send to X3  F9 Wi-Fi",
        "",
        "Files live on the SD card; only your edits",
        "use RAM, so huge files are fine.",
        "Press any key",
    };
    const Theme& t = ui.th();
    const Font& f = *kFonts[0];
    gfx::fill(0, 0, LCD_W_MAX, LCD_H_MAX, t.panelBg);
    int y = 4;
    for (const char* l : kLines) {
        gfx::drawText(6, y, LCD_W_MAX - 12, f, l, l == kLines[0] ? t.accent : t.panelFg, t.panelBg);
        y += f.h;
    }
    ui.kbd.wait();
}

}  // namespace dlg
