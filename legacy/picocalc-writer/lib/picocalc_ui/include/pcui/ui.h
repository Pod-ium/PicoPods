// picocalc_ui - reusable menus and dialogs for PicoCalc firmware.
//
// Every dialog draws a panel over whatever is on screen and blocks on the
// keyboard through the Input interface you provide; repaint afterwards.
//
//   pcui::MenuItem items[] = {
//       pcui::action("Resume"),
//       pcui::action("Save", "F2"),                          // right-aligned hint
//       pcui::choice("Palette", &palette, names, 4),         // Left/Right cycle
//       pcui::range("Volume", &volume, 0, 100, 10, "%d%%"),  // Left/Right step
//   };
//   int picked = pcui::menu(input, theme, "PAUSED", items);  // -1 = Esc
#pragma once
#include <cstddef>
#include <cstdint>
#include "pcui/gfx.h"

namespace pcui {

// Key codes reported by the PicoCalc keyboard firmware.
namespace key {
constexpr int Backspace = 0x08, Tab = 0x09, Enter = 0x0A, Esc = 0xB1;
constexpr int Left = 0xB4, Up = 0xB5, Down = 0xB6, Right = 0xB7;
constexpr int Home = 0xD2, Del = 0xD4, End = 0xD5, PageUp = 0xD6, PageDown = 0xD7;
constexpr int F1 = 0x81, F2 = 0x82, F3 = 0x83, F4 = 0x84, F5 = 0x85;
}  // namespace key

// Supplied by the application (each firmware reads the keyboard its own way).
class Input {
public:
    virtual ~Input() = default;
    virtual int wait_key() = 0;   // blocks; returns a key code (with auto-repeat if desired)
};

struct Theme {
    Color bg, fg, dim;             // screen
    Color panel_bg, panel_fg;      // dialogs
    Color hi_bg, hi_fg;            // selected row
    Color accent, error;
};
extern const Theme THEME_NIGHT;    // the PicoCalc Writer "Night" colours

// ---------------------------------------------------------------- menu
struct MenuItem {
    enum Kind : uint8_t { ACTION, CHOICE, RANGE } kind = ACTION;
    const char* label = "";
    const char* hint = nullptr;             // ACTION: right-aligned text (e.g. a shortcut)
    int* value = nullptr;                   // CHOICE / RANGE
    const char* const* options = nullptr;   // CHOICE
    int count = 0;                          // CHOICE
    int min = 0, max = 0, step = 1;         // RANGE
    const char* fmt = "%d";                 // RANGE
    bool enabled = true;
};

MenuItem action(const char* label, const char* hint = nullptr);
MenuItem choice(const char* label, int* value, const char* const* options, int count);
MenuItem range(const char* label, int* value, int min, int max, int step = 1, const char* fmt = "%d");

struct MenuOptions {
    int initial = 0;
    const char* footer = nullptr;                       // dim line under the items
    void (*on_change)(int index, void* user) = nullptr; // CHOICE/RANGE value changed
    void* user = nullptr;
};

// Up/Down move, Enter picks an ACTION (or advances a CHOICE/RANGE), Left/Right
// change a CHOICE/RANGE, Esc/Backspace cancel. Returns the picked index or -1.
int menu(Input& in, const Theme& t, const char* title, const MenuItem* items, int count,
         const MenuOptions& opt = MenuOptions());

template <size_t N>
int menu(Input& in, const Theme& t, const char* title, const MenuItem (&items)[N],
         const MenuOptions& opt = MenuOptions()) {
    return menu(in, t, title, items, int(N), opt);
}

// ---------------------------------------------------------------- dialogs
// Shows up to 8 lines and waits for a key.
void message(Input& in, const Theme& t, const char* title, const char* const* lines, int n,
             Color line_color = 0xFFFFFFFF /* = theme panel_fg */);
// Returns true for Enter / 'y'.
bool confirm(Input& in, const Theme& t, const char* title, const char* question);
// Non-blocking progress panel; call repeatedly with increasing `done`.
void progress(const Theme& t, const char* title, const char* label, uint32_t done, uint32_t total);

// ---------------------------------------------------------------- file browser (FatFs)
using FileFilter = bool (*)(const char* name);   // return true to list a file

struct BrowseOptions {
    FileFilter filter = nullptr;          // nullptr = all files
    const char* select = nullptr;         // entry to preselect in the starting folder
    const char* empty_hint = nullptr;     // shown when a folder has nothing to list
    bool allow_cancel = true;             // Esc returns false
    bool show_size = true;
};

// Browses from `dir` (updated as the user navigates). Returns true with the
// chosen file's full path in `out`.
bool browse(Input& in, const Theme& t, const char* title, char* dir, size_t dir_cap, char* out,
            size_t out_cap, const BrowseOptions& opt = BrowseOptions());

}  // namespace pcui
