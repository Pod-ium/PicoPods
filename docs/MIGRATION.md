# Moving PicoCalc Writer and PicoBoy onto the common libraries

Both firmwares have been ported: see `pods/podwriter` (Writer 3.0, now Pod Writer) and `pods/podboy` (PicoBoy 1.1, now PodBoy). The original 2.1.0 / 1.0 sources are in the repository history (commits "Import PicoCalc Writer 2.1.0" and "Import PicoBoy 1.0", folder `legacy/`, removed once the pods were in place). This is the map from their original code to the libraries, useful to follow the port or to move other PicoCalc firmware the same way.

## PicoCalc Writer 2.1.0

| Writer | PicoPods | Notes |
|---|---|---|
| `src/hw/board.h` | `pd/terminals/picocalc.h` | Same pins and tunables, as `PD_PICOCALC_*` macros. |
| `src/hw/lcd.*` (wrapper over picocalc_ui) | `pd::gfx` + terminal display | Colours become `0xRRGGBB`; `lcd::rgb(r,g,b)` → `pd::rgb(r,g,b)`, RGB565 constants → `pd::from565()`. |
| `src/hw/keyboard.*` | `pd::input::Keyboard` (`terminal.keyboard()`) | `KeyEvent` has the same fields plus `repeat`; `kbd_.battery()` → `terminal.battery_percent()` / `power()->charging()`. |
| `src/hw/sdcard.cpp` | `pd::drivers::SdSpi` | Same driver, plus card detect. |
| `src/editor/storage.*` | `pd/fs.h` | `isOpen` → `is_open`, `lastErrorText` → `last_error_text`, `listDir` → `list_dir`, `isDir` → `is_dir`. New: `Mode::Append/Update`, `stat`, `mkdirs`, `read_file/write_file`. |
| `src/editor/text_buffer.*`, `layout.*`, `syntax.*` | `pd_text`: `pd::text::TextBuffer`, `pd::text::Layout`, `pd::syn::` | Code unchanged apart from the namespace and fs names. |
| Theme syntax colours in `src/ui/gfx.cpp` | `pd::syn::palette(theme)` | |
| `src/ui/gfx.*` (`drawCells`, `drawText`) | `pd::gfx::draw_cells`, `pd::gfx::text` | Same renderer; `Cell` colours are `pd::Color`. |
| `src/ui/dialogs.*` | `pd::ui::prompt`, `ask` (3-way confirm), `message`, `browse`, `text_view` (help) | `dlg::menu(ui_, …)` → `pd::ui::menu(ctx, …)`; path helpers → `pd::path`. Hide the work folder with `BrowseOptions::hide = {".fwedit"}` (dot folders are hidden by default anyway). |
| Themes (`Night`, `Paper`, `Phosphor`) | `pd::ui::THEME_*`, `pd::ui::theme(i)` | Fields renamed to snake_case (`panelBg` → `panel_bg`, `panelHi` → `hi_bg`…). |
| `loadSettings/saveSettings` parsing | `pd::Settings` | Same file format, so existing `/.fwedit/settings.txt` files still load. |
| `src/diag.*` | `pd_diag` | `diag::begin("PicoCalc Writer", FW_VERSION, names, n)`; the stage enum stays in the app. `PICO_PANIC_FUNCTION` is set by the library. |
| `src/net/net.*`, `lwipopts.h` | `pd_net` | `isOn` → `is_on`, `ipAddress` → `ip_address`, `ipToString` → `ip_to_string`. |
| Wi-Fi flow in `app_share.cpp` (`chooseNetwork`, `ensureWifi`, `cmdWifi`, idle off) | `pd::WifiManager` (`pd_wifi`) | Pass `"/.fwedit/wifi.txt"` to keep existing credentials. Call `wifi.tick()` from the main loop, or set `ui.idle`. |
| `src/net/x3.*` | `pd_crosspoint` (`pd::crosspoint::`) | `parseUrl` → `parse_url`, `urlEncode` → `url_encode`, `isAlreadyExists` → `is_already_exists`. |
| `src/export/epub.*` | `pd_epub` (`pd::epub::`) | Unchanged API. |
| `test/` | `tests/` | All Writer tests are in the PicoPods test suite. |

What stays in Pod Writer: `App` (editor state, key bindings, rendering loop, status bar content, commands), the help text, and the ePub/X3 command flow.

## PicoBoy 1.0

| PicoBoy | PicoPods | Notes |
|---|---|---|
| `src/config.h` hardware section | `pd/terminals/picocalc.h` | `AUDIO_PIN_L 26` → `PD_PICOCALC_AUDIO_L=26`. `SYS_CLOCK_KHZ` → `TerminalConfig::clock_khz`. |
| `src/hw/keyboard.*` + `core1_main` | `TerminalConfig::keyboard_on_core1 = true`, then `kbd.poll_raw()` | `KeyEvent{key,state}` → `hal::RawKey{code,state}`; letters are not lower-cased: use `pd::key::lower(code)` in `map_key`. |
| `KbdInput` (auto-repeat for menus) | `pd::input::Keyboard::poll/wait` | Delete it; pass `ui::Context` to the menus. |
| `src/hw/video.*` | `pd::IndexedBlitter` | `begin(display, {160, 144, 2, 0, GAME_Y})`, `set_palette(rgb, 4)`, `submit/poll/busy/wait/invalidate` map one to one. |
| `src/hw/audio.*` | `terminal.audio()` (`PwmAudio`) | `audio_init()` → `audio->start(gb::AUDIO_RATE)`; `audio_push/buffered/silence/set_volume` map one to one. |
| `src/hw/romstore.*` | `pd_flash_cache` | `romstore_init()` → `flash_cache::init(ROMCACHE_MAX_BYTES)`, `romstore_load(path, &size, progress, user, &err)` → `flash_cache::load(path, &size, progress, &err)`. Validity uses the first 512 bytes instead of the GB header range. |
| `src/hw/sdcard.*`, `lib/fatfs` | `pd_fs` | The browser and `.sav` code use `pd::fs::File`. |
| `lib/picocalc_ui` | `pd_ui`, `pd_gfx` | `pcui::menu(input, theme, …)` → `pd::ui::menu(ctx, …)`; `on_change` is a `std::function<void(int)>`; `pcui::message(lines, n)` → `pd::ui::message(ctx, title, {…})`; `browse(char* dir …)` → `browse(ctx, title, std::string& dir, std::string& out, opts)` with `opts.filter = ui::extension_filter(".gb,.gbc")`. |
| `mount_sd()` | `pd::app::mount_storage(ui)` | |
| Splash with colour bars | `pd::app::splash(theme, "PicoBoy", "Game Boy emulator", true)` | |
| `key_name()` | `pd::key::name()` | |

What stays in PicoBoy: the emulator core (`src/gb/`), key bindings, palettes, the pause menu, `.sav` handling and the frame/audio pacing loop.

Keep in mind that dialogs do not flush the keyboard: call `ui.kbd.flush()` before opening the pause menu, as PicoBoy already does.

## From picodev to PicoPods

PicoPods is picodev 2 renamed and reorganised. An app written for picodev
moves over with these changes; library targets (`pd_*`), headers (`pd/...`),
the `pd::` namespace and the `PD_*` options are the same.

| picodev | PicoPods |
|---|---|
| `include(picodev/cmake/picodev_init.cmake)` | `include(picopods/cmake/picopods_init.cmake)` |
| `include(picodev/cmake/picodev.cmake)` | `include(picopods/cmake/picopods.cmake)` |
| `picodev_add_app(name …)` | `picopods_add_pod(name …)` (same arguments) |
| `PICODEV_ROOT` | `PICOPODS_ROOT` |
| `lib/<name>` | `lib/<layer>/<name>`, see [`lib/README.md`](../lib/README.md) |
