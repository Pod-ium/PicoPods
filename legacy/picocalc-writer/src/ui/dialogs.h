// Modal dialogs. Each one draws over the screen and blocks on the keyboard;
// the caller must repaint afterwards.
#pragma once
#include <string>
#include <vector>

#include "../hw/keyboard.h"
#include "gfx.h"
#include "pcui/ui.h"

struct Ui {
    Keyboard& kbd;
    int themeIdx;
    const Theme& th() const { return gfx::theme(themeIdx); }
};

namespace dlg {

// Menus come from the shared picocalc_ui library (pcui::menu): items are
// pcui::action(label, hint), pcui::choice(label, &value, names, count) and
// pcui::range(...), and can be disabled (item.enabled = false). This wrapper
// feeds it Writer's keyboard and current theme. Returns the picked index or -1.
int menu(Ui& ui, const char* title, const std::vector<pcui::MenuItem>& items,
         const pcui::MenuOptions& opt = pcui::MenuOptions());
// The picocalc_ui version of a Writer theme (same colours).
pcui::Theme pcuiTheme(const Theme& t);
// Single-line text input. Returns false if cancelled. mask: shows '*'
// (Tab reveals / hides the text), for passwords.
bool prompt(Ui& ui, const char* title, std::string& value, const char* hint = nullptr,
            bool mask = false);
// Returns 1 = yes, 0 = no, -1 = cancel.
int confirm(Ui& ui, const char* title, const std::string& text, bool allowCancel = true);
void message(Ui& ui, const char* title, const std::vector<std::string>& lines);
// File browser starting in `cwd` (updated on return). Returns false if cancelled.
bool browse(Ui& ui, std::string& cwd, std::string& outPath, const char* title);
void help(Ui& ui);

// Path helpers
std::string join(const std::string& dir, const std::string& name);
std::string parent(const std::string& path);
std::string baseName(const std::string& path);

}  // namespace dlg
