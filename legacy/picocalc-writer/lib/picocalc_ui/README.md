# picocalc_ui

Shared display and UI code for ClockworkPi PicoCalc firmware (Pico SDK, C++17).
Extracted from PicoCalc Writer and PicoBoy.

| Module | Contents |
|---|---|
| `pcui/lcd.h` | LCD driver for the 320×320 ST7365P panel: init, windows, blocking and DMA writes, fills |
| `pcui/gfx.h` | Terminus fonts (6×12, 8×16, 10×20, 12×24), UTF-8/Latin-1 text, rectangles |
| `pcui/ui.h` | Generic menu, message box, yes/no confirm, progress bar, FatFs file browser |

## Using it

```cmake
add_subdirectory(lib/picocalc_ui)
target_link_libraries(my_app picocalc_ui)
# the browser needs FatFs: ff.h / ffconf.h must be on my_app's include path
```

The library does not read the keyboard itself: each firmware reads it differently. Instead you implement one method:

```cpp
class MyInput : public pcui::Input {
    int wait_key() override { /* block until a key, return its code */ }
};
```

## Menus

A menu is an array of items. Three kinds exist:

- **`action(label, hint)`**: Enter returns its index. The optional hint is right-aligned, useful for shortcuts.
- **`choice(label, &value, names, count)`**: Left/Right cycles through the options.
- **`range(label, &value, min, max, step, fmt)`**: Left/Right steps the number.

```cpp
int palette = 0, volume = 60;
const char* const names[] = {"Green", "Grey"};
pcui::MenuItem items[] = {
    pcui::action("Resume", "Esc"),
    pcui::action("Quit"),
    pcui::choice("Palette", &palette, names, 2),
    pcui::range("Volume", &volume, 0, 100, 10, "%d%%"),
};
pcui::MenuOptions opt;
opt.footer = "Game saved";
opt.on_change = [](int index, void*) { /* apply the new value live */ };
int picked = pcui::menu(input, pcui::THEME_NIGHT, "PAUSED", items, opt);   // -1 = Esc
```

Values are changed in place through the pointers. `on_change` fires on every change, so you can apply settings (like volume) immediately.

## Themes

`pcui::Theme` holds nine colours as `0xRRGGBB`. `THEME_NIGHT` reproduces the Writer's default theme; define your own for other looks.

## Display notes

- The panel only accepts 18-bit colour over SPI (3 bytes per pixel). The 16-bit command is silently ignored.
- The window setup and pixel data must share one chip-select transaction.
- CS and DC changes need a few hundred nanoseconds to settle. The driver pads every edge; without this the panel misreads commands.

## Licences

The Terminus font is © Dimitar Toshkov Zhekov, under the SIL Open Font License 1.1.
