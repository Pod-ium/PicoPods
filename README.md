# PicoPods

Firmware for the **ClockworkPi PicoCalc** with a Raspberry Pi **Pico 2 W**.

PicoPods is one tree with two parts:

- a **common set of libraries** (`lib/`) that drive the PicoCalc hardware
  (display, keyboard, SD card, audio, battery, Wi-Fi, Bluetooth, USB) and
  provide the shared pieces every firmware needs (files, fonts, menus,
  dialogs, crash screen);
- a set of firmwares built on them, called **pods** (`pods/`):

| Pod | What it is |
|---|---|
| **PodBoy** | Game Boy (DMG) emulator |
| **Pod Writer** | Text editor with syntax highlighting, for files larger than the Pico's RAM (SD-backed) |
| **Pod Files** | File manager for the SD card |

The tree is being assembled from the original standalone firmwares; see
[`docs/ROADMAP.md`](docs/ROADMAP.md) for milestones, stages and gates.
