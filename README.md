# PicoPods

[![CI](https://github.com/lando1784/PicoPods/actions/workflows/ci.yml/badge.svg)](https://github.com/lando1784/PicoPods/actions/workflows/ci.yml)

Firmware for the **ClockworkPi PicoCalc** with a Raspberry Pi **Pico 2 W**.

PicoPods is one tree with two parts:

- a **common set of libraries** ([`lib/`](lib/README.md)) that drive the
  PicoCalc hardware (display, keyboard, SD card, audio, battery, Wi-Fi,
  Bluetooth, USB) and provide what every firmware needs (files, fonts,
  menus, dialogs, crash screen, editor engine);
- the firmwares built on them, called **pods**.

| Pod | Folder | What it is |
|---|---|---|
| **PodBoy** 1.1 | [`pods/podboy`](pods/podboy) | Game Boy (DMG) emulator. Needs an RP2350 |
| **Pod Writer** 3.0 | [`pods/podwriter`](pods/podwriter) | Text editor: files larger than RAM (SD-backed), undo, syntax colours, ePub export |
| **Pod Files** 1.3 | [`pods/podfiles`](pods/podfiles) | File manager: copy / move / delete, Bluetooth, Git clone, USB drive mode |

Examples (`examples/`): `pd_demo` (tour of the libraries and hardware
check), `pd_template` (skeleton to copy for a new pod) and `pd_usbprobe`
(check of the Pico's micro-USB port).

How the tree was assembled from the original firmwares, milestone by
milestone: [`docs/ROADMAP.md`](docs/ROADMAP.md).

## Build

```sh
# Firmware for PicoCalc + Pico 2 W (default)
cmake -B build -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build -j
# -> build/pods/<pod>/<pod>.uf2, build/examples/<example>/<example>.uf2

# Other boards
cmake -B build-pico -DPD_BOARD=pico        # also pico_w, pico2, or any Pico SDK board name

# PC build: simulator + tests
cmake -B build-host -DPD_TERMINAL=host -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build-host -j && ctest --test-dir build-host

# Both merge gates in one go
PICO_SDK_PATH=/path/to/pico-sdk tools/gate.sh all
```

Needs Pico SDK 2.1.x, `arm-none-eabi-gcc` and CMake ≥ 3.13. The host build
needs any C++17 compiler, Python 3, and `libmbedtls-dev` for the TLS/Git
tests; `PICO_SDK_PATH` there enables the Bluetooth tests (BTstack sources).

Ready-to-flash `.uf2` files for every board are attached to each CI run
(artifacts `picopods-<board>`). Flash the `.uf2` in BOOTSEL mode, or run it from the SD card with
[UF2 Loader](https://github.com/pelrun/uf2loader): copy it to `pico2-apps/`
(Pico 2 / 2 W) or `pico1-apps/` (Pico / Pico W) and pick it in the loader's
menu. All pods and examples are plain Pico SDK images, which the loader runs
unchanged; the only code that writes flash (`pd_flash_cache`: PodBoy's ROM
cache, the demo's flash test) stays inside the app partition on the RP2350
and below the loader's top 16 KB on the RP2040. Bluetooth pairings live on the
SD card, never in flash. **First run on new hardware:** flash `pd_demo.uf2`
and go through its keyboard, colour, audio and flash-cache screens.

## Releases

Publishing a GitHub release runs `.github/workflows/release.yml`, which
builds the three pods for PicoCalc + Pico 2 W and attaches them to the
release as `podboy_<version>.uf2`, `podwriter_<version>.uf2` and
`podfiles_<version>.uf2` (tag `v1.2.0` gives version `1.2.0`), plus a
`SHA256SUMS_<version>.txt`. The same files can be built locally with
`PICO_SDK_PATH=… tools/package_release.sh <version>` (output in `dist/`).
The workflow can also be started by hand from the Actions tab for an
existing tag, to rebuild or replace its files.

## Write a pod

Copy `examples/template` to `pods/<name>`, rename it and add it to the top-level `CMakeLists.txt`:

```cmake
picopods_add_pod(my_pod
    SOURCES main.cpp
    LIBS pd_diag pd_text          # anything beyond the core set
    TITLE "My Pod" VERSION "1.0")
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
        if (pd::ui::menu(ui, "MY POD", items) == 0) {
            std::string dir = "/", file;
            pd::ui::browse(ui, "Open", dir, file);
        }
    }
}
```

From another repository, add PicoPods as a git submodule:

```cmake
cmake_minimum_required(VERSION 3.13)
include(picopods/cmake/picopods_init.cmake)   # before project(): board, terminal, SDK import
project(my_pod C CXX ASM)
include(picopods/cmake/picopods.cmake)        # after project(): SDK init, libraries
picopods_add_pod(my_pod SOURCES main.cpp)
```

## Layout

```
cmake/        picopods_init.cmake (before project), picopods.cmake (after project)
lib/          common libraries, by layer: hardware/ services/ interface/ content/
pods/         podboy/ podwriter/ podfiles/
examples/     demo/ template/ usbprobe/
tests/        host tests (ctest) and their data
tools/        gate.sh (merge gates), package_release.sh (release .uf2), gen_fonts.py (font data)
docs/         ARCHITECTURE, PORTING, MIGRATION, ROADMAP
```

## Status

- Builds cleanly for `pico2_w`, `pico2`, `pico_w` and `pico` (PodBoy is
  skipped on RP2040) and for the host; CI checks all of them.
- Host tests (24) pass: editor engine fuzz test with ASan/UBSan, syntax
  lexer, ePub samples with structure checks, X3 upload against a mock
  server, core libraries (files, settings, input, drawing, dialogs,
  blitter, flash cache), tree operations, the Bluetooth OBEX receiver and
  simulated Bluetooth sessions, the BTstack lifecycle, and scripted
  sessions of the demo and the three pods. Git clones of real
  repositories run with `-DPD_NET_TESTS=ON` (needs network).
- The drivers come from Writer and PicoBoy code that ran on real PicoCalc
  hardware, but **the pods in this tree have not been run on a device
  yet**. New and untested on hardware: multi-block SD writes (they fall
  back to single-block writes on error) and Pod Files' USB drive mode.
  The first hardware session should go through the demo.

See [`CHANGELOG.md`](CHANGELOG.md) for what changed from the original firmwares.
