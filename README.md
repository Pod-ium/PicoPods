# PicoPods

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
| PicoCalc Files 1.2 | `apps/files` | File manager: copy / move / delete, Bluetooth, Git clone, USB drive mode |
| `pd_demo` | `apps/demo` | Tour of the libraries and hardware check |
| `pd_template` | `apps/template` | Skeleton to copy for a new pod |
| `pd_usbprobe` | `apps/usbprobe` | Check of the Pico's micro-USB port |

The pods are being renamed and moved to `pods/` (PodBoy, Pod Writer,
Pod Files): see [`docs/ROADMAP.md`](docs/ROADMAP.md).

## Build

```sh
# Firmware for PicoCalc + Pico 2 W (default)
cmake -B build -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build -j
# -> build/pods/<pod>/<pod>.uf2 (and build/apps/... until M3 is done)

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

Flash the `.uf2` in BOOTSEL mode, or copy it to the SD card folder used by
the PicoCalc UF2 Loader. **First run on new hardware:** flash `pd_demo.uf2`
and go through its keyboard, colour, audio and flash-cache screens.

## Write a pod

Copy `apps/template`, rename it and add it to the top-level `CMakeLists.txt`:

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
apps/         the pods and examples (moving to pods/ and examples/)
tests/        host tests (ctest) and their data
tools/        gate.sh (merge gates), gen_fonts.py (font data)
docs/         ARCHITECTURE, PORTING, MIGRATION, ROADMAP
legacy/       the original standalone Writer 2.1.0 and PicoBoy 1.0 (reference)
```

## Status

Builds cleanly for `pico2_w`, `pico2`, `pico_w` and `pico` (PicoBoy is
skipped on RP2040) and for the host, where the whole test suite passes.
The drivers come from Writer and PicoBoy code that ran on real PicoCalc
hardware, but the ported pods have not been run on a device yet: the first
hardware session should go through the demo.
