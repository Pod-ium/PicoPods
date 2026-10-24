# picodev

A reusable base for C++ firmware on the **ClockworkPi PicoCalc** with a Raspberry Pi **Pico 2 W**, built from the parts of *PicoCalc Writer* and *PicoBoy* that every app needs: display, fonts, keyboard, SD card, audio, menus and dialogs, Wi-Fi, flash cache, crash screen, editor engine and ePub export.

Hardware access sits behind a small abstraction layer, so the same app code can target other Pico-format boards (Pico, Pico W, Pico 2) and, with one new library, other *terminals* (devices that host a programmable board, like the PicoCalc). A PC terminal is included: apps and tests run headless on Linux/macOS with scripted keys and screenshots.

## Quick start

```sh
# Firmware for PicoCalc + Pico 2 W (default)
cmake -B build -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build -j
# -> build/apps/demo/pd_demo.uf2, build/apps/template/pd_template.uf2

# Other boards
cmake -B build-pico -DPD_BOARD=pico        # also pico_w, pico2, or any Pico SDK board name

# PC build: simulator + tests
cmake -B build-host -DPD_TERMINAL=host
cmake --build build-host -j && ctest --test-dir build-host
```

Needs Pico SDK 2.1.x, `arm-none-eabi-gcc`, CMake ≥ 3.13 (host build: any C++17 compiler, Python 3 for two tests). FatFs is vendored, nothing is downloaded at configure time apart from what the Pico SDK itself fetches (picotool, if not installed).

Flash the `.uf2` in BOOTSEL mode, or copy it to the SD card folder used by the PicoCalc UF2 Loader.

## Apps in this tree

| App | Folder | What it is |
|---|---|---|
| **PicoCalc Writer 3.0** | `apps/writer` | Distraction-free text editor: files larger than RAM, undo, syntax colours, ePub export, upload to an Xteink X3. Port of Writer 2.1 onto picodev. |
| **PicoCalc Files 1.2** | `apps/files` | File manager: copy / cut / paste, delete, rename, new folder, multi-select, Bluetooth file send/receive, Git clone over Wi-Fi, and a USB drive mode (not working yet on hardware). |
| **USB probe** | `apps/usbprobe` | Hardware check for the Pico's micro-USB port: the SDK's stock USB serial plus clock/controller readout. |
| **PicoBoy 1.1** | `apps/picoboy` | Game Boy (DMG) emulator with ROM browser, saves, pause menu. Port of PicoBoy 1.0 onto picodev. Needs an RP2350. |
| `pd_demo` | `apps/demo` | Tour of the libraries and hardware check. |
| `pd_template` | `apps/template` | Skeleton to copy for a new app. |

Ready-to-flash `.uf2` files for PicoCalc + Pico 2 W are in `prebuilt/pico2_w/`.

**First run on new hardware:** flash `pd_demo.uf2`. *Keyboard test* shows raw events and modifiers, *Fonts and colours* ends with red/green/blue bars (if they look wrong, see `PD_PICOCALC_LCD_INVERT`), *Blitter + audio* checks DMA and PWM audio (Space toggles a tone), *Flash cache test* and *System info* check the SD card and flash.

## Start a new app

Copy `apps/template` next to the other apps (or into your own repository, see below), rename it and add it to the top-level `CMakeLists.txt`:

```cmake
picodev_add_app(my_app
    SOURCES main.cpp
    LIBS pd_diag pd_text          # anything beyond the core set
    TITLE "My App" VERSION "1.0")
```

```cpp
#include "pd/pd.h"

int main() {
    pd::TerminalConfig cfg;
    cfg.clock_khz = pd::board::fast_clock_khz();     // 200 MHz, optional
    pd::Terminal& t = pd::terminal_init(cfg);
    pd::gfx::init(t.display());
    pd::ui::Context ui(t.keyboard(), pd::ui::THEME_NIGHT);
    pd::app::mount_storage(ui);                      // "Insert an SD card" until it works

    int volume = 60;
    pd::ui::MenuItem items[] = {
        pd::ui::action("Open file"),
        pd::ui::range("Volume", &volume, 0, 100, 10, "%d%%"),
    };
    for (;;) {
        if (pd::ui::menu(ui, "MY APP", items) == 0) {
            std::string dir = "/", file;
            pd::ui::browse(ui, "Open", dir, file);
        }
    }
}
```

**Using picodev from another repository:** add it as a git submodule and write

```cmake
cmake_minimum_required(VERSION 3.13)
include(picodev/cmake/picodev_init.cmake)   # before project(): board, terminal, SDK import
project(my_app C CXX ASM)
include(picodev/cmake/picodev.cmake)        # after project(): SDK init, libraries
picodev_add_app(my_app SOURCES main.cpp)
```

## Libraries

Every library is a CMake `INTERFACE` target, compiled into the app that links it (the Pico SDK convention), so only what an app uses ends up in its firmware.

| Target | Header(s) | What it gives you | Origin |
|---|---|---|---|
| `pd_core` | `pd/core.h`, `pd/sys.h`, `pd/board.h` | Colours, time, sleep, clock/overclock with voltage, core-1 launch (flash-safe), reboot/BOOTSEL, board capabilities | new |
| `pd_hal` | `pd/hal/*.h` | Interfaces: `Display`, `KeyboardDevice`, `AudioOut`, `BlockDevice`, `Power` | new |
| `pd_input` | `pd/input.h`, `pd/keys.h` | Raw layer (press/release, key state) for games, cooked layer (modifiers, auto-repeat) for UIs, optional core-1 polling, Esc checker | Writer + PicoBoy |
| `pd_fs` | `pd/fs.h`, `pd/path.h`, `pd/settings.h`, `pd/fs_ops.h` | Files and folders (FatFs R0.16, UTF-8 names, exFAT), atomic writes, path helpers, `key=value` settings; recursive copy / move / delete with conflicts, progress and cancel; copies keep date and attributes | Writer + Files |
| `pd_gfx` | `pd/gfx.h`, `pd/blit.h` | 4 Terminus fonts (Latin-1), text, per-character colours with caret, shapes, RGB blits; indexed framebuffer → scaled DMA blitter with dirty-line detection | picocalc_ui + Writer + PicoBoy |
| `pd_ui` | `pd/ui.h` | 3 themes, menus (action/choice/range/info), message, confirm, yes/no/cancel, text prompt (password mode), progress, busy panel, scrollable text view, file browser | picocalc_ui + Writer |
| `pd_app` | `pd/pd.h`, `pd/app.h` | Umbrella include, SD mount screen, splash, status bar with battery | new |
| `pd_diag` | `pd/diag.h` | Start-up log, reset breadcrumb ("previous start stopped at…", safe start), crash screen for HardFault and `panic()` | Writer |
| `pd_flash_cache` | `pd/flash_cache.h` | Copy a file from SD into flash and read it memory-mapped; UF2 Loader partition aware | PicoBoy |
| `pd_net` | `pd/net.h` | Wi-Fi scan/connect, DNS + mDNS, blocking TCP client (CYW43 + lwIP poll mode) | Writer |
| `pd_wifi` | `pd/wifi.h` | Saved credentials, network picker UI, connect on demand, idle power-off | Writer |
| `pd_crosspoint` | `pd/crosspoint.h` | Upload/delete/status for Xteink X3/X4 readers running CrossPoint | Writer |
| `pd_text` | `pd/text/*.h` | Piece-table text buffer on SD (files larger than RAM, undo/redo), soft-wrap layout, streaming syntax lexer (Markdown, XML, Python, C/C++) | Writer |
| `pd_epub` | `pd/epub.h` | Streaming EPUB 2.0.1 writer (Markdown → headings/lists/tables/code, chapters and TOC) | Writer |
| `pd_tls` | `pd/tls.h` | TLS client (mbedTLS 2.28) over `pd::net::Tcp` with built-in public root CAs, and an HTTP/1.1 client (http/https, redirects, chunked bodies) | Files |
| `pd_git` | `pd/git.h` | Git clone over HTTPS (smart HTTP, shallow or full) onto the SD card: pack streamed to the card, deltas resolved with bounded RAM, real `.git` (pack, .idx, refs, index) | Files |
| `pd_miniz` | `miniz.h` | zlib inflate/deflate (miniz 3.0.2, MIT, third party) | Git |
| `pd_bt` | `pd/bt.h` | Bluetooth file transfer (OBEX Object Push) both ways: discovery, numeric-comparison pairing, receive into a folder, send files; pairing keys in a file on the SD card (no flash writes); BTstack Classic on the CYW43; simulator backend on the host | Files |
| `pd_usb_drive` | `pd/usb_drive.h` | USB drive mode: the SD card as a USB mass-storage device on the board's native USB (TinyUSB MSC), with the file system unmounted meanwhile; full-screen status and eject handling | Files |
| `pd_drivers` | `pd/drivers/*.h` | ST7365P panel, SD over SPI (multi-block reads and writes), PicoCalc keyboard/battery controller, PWM stereo audio | picocalc_ui + Writer + PicoBoy |
| `pd_terminal` | `pd/terminal.h` | The selected terminal: `terminal_init()` wires drivers to pins | new |

`pd_net`, `pd_wifi` and `pd_crosspoint` have a real implementation only on Wi-Fi boards (and the host); CMake sets `PD_HAS_WIFI` so apps can link them conditionally (see `apps/demo/CMakeLists.txt`).

## Layout

```
cmake/            picodev_init.cmake (before project), picodev.cmake (after project)
lib/              one folder per library (include/ + src/ + CMakeLists.txt)
  terminals/      picocalc/ and host/, plus the shared pd/terminal.h
apps/template/    minimal app to copy
apps/demo/        tour of every library, hardware check
tests/            host tests (ctest), Writer's test data, mock CrossPoint server
tools/gen_fonts.py  regenerates the font data from Terminus PSF files
docs/             ARCHITECTURE.md, PORTING.md, MIGRATION.md
```

## Status

- Builds cleanly for `pico2_w`, `pico2` and `pico` (RP2040), and for the host. PicoBoy is skipped on RP2040.
- Host tests pass: editor engine fuzz test, syntax lexer, ePub (7 samples, structure checks), X3 upload against the mock server, core library tests (files, settings, input, pixel-level drawing, dialogs, blitter, flash cache), tree operations (copy/move/delete, conflicts, cancel), the Bluetooth OBEX receiver (real OBEX packets, any chunking) and a simulated Bluetooth receive session, Git clones of real GitHub repositories checked with `git fsck --strict` (with `-DPD_NET_TESTS=ON`; needs network, `git` and `libmbedtls-dev`), and scripted sessions of the demo, the Writer, PicoBoy and PicoCalc Files (checked against the resulting card contents). On the host PicoBoy passes Blargg's `cpu_instrs` and renders dmg-acid2 correctly (the ROMs are not included).
- The drivers and the Writer/PicoBoy code paths they come from ran on real PicoCalc hardware, but **picodev, the ported Writer 3.0 / PicoBoy 1.1 and PicoCalc Files have not been run on a device yet**. New and untested on hardware: multi-block SD writes (they fall back to single-block writes on error) and the USB drive mode. The first hardware session should go through the demo; see *First run* above.

## Licences

- FatFs (ChaN): BSD-style, `lib/fs/fatfs/LICENSE.txt`.
- Terminus Font (Dimitar Toshkov Zhekov): SIL Open Font License 1.1.
- Display init sequence and pin map from ClockworkPi's reference code.
