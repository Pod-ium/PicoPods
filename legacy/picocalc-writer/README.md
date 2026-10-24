# PicoCalc Writer

A distraction-free, bare-metal C++ text editor firmware for the **ClockworkPi PicoCalc** with a **Raspberry Pi Pico 2 W** (RP2350).
It boots straight into the last file you were editing, at the same cursor position, and gets out of your way.

Files are edited **directly on the SD card**, so documents far larger than the Pico's 520 KB of RAM (up to 2 GB) open instantly and edit smoothly.

## What's new in 2.1

- **Shared UI library.** The display driver, the fonts and the menus now come from `lib/picocalc_ui`, the same library PicoBoy uses (see `lib/picocalc_ui/README.md`). A fix or improvement made there benefits both firmwares.
- **Settings in the menu.** Font size, theme, status bar and syntax colours appear in the Esc menu with their current value (`< 8x16 >`); change them with Left/Right. They are applied and saved when the menu closes.
- **Clearer Wi-Fi menu.** The status lines are shown dimmed and skipped by the cursor. "Turn Wi-Fi off" and "Forget saved network" are only selectable when they apply.
- **Less memory.** The fonts are stored once in flash (2.0 had some of them two or three times). The ePub export needs about 4 KB less stack.

## Features

- New, Open (file browser), Save, Save As (with overwrite confirmation), unsaved-changes prompts
- Undo / redo (typing grouped by word), cut / copy / paste of any size (clipboard lives on the SD card)
- Find, find next, replace all (single undo step), go to line, document statistics (words, lines, chars)
- Soft word-wrap, UTF-8 aware (Latin-1 glyphs displayed), auto-indent, insert / overwrite mode
- Four Terminus font sizes (6×12, 8×16, 10×20, 12×24), three themes (Night, Paper, Phosphor)
- ePub export (Markdown becomes real headings, emphasis, lists, quotes, tables and code; chapters and table of contents from `#` / `##` headings)
- Wi-Fi (Pico 2 W) and one-key upload of the ePub to an Xteink X3 / X4 running CrossPoint
- Syntax colours for Markdown, XML (also URDF, xacro, SDF, SVG, HTML), Python and C/C++, chosen by file extension; F6 toggles them
- Esc menu with every command, and the display settings as Left/Right choices
- Optional one-line status bar: file name, modified mark, language, OVR, position %, battery
- Remembers font, theme, status bar, syntax colours, current folder, last file and cursor position

## Keys

| Key | Action |
|---|---|
| Esc | Menu (all commands) |
| F1 | Help |
| F2 / F4 / F5 | Font size / status bar / theme |
| F3 | Find next |
| F6 | Syntax colours on / off |
| F7 / F8 / F9 | Export ePub / Send to X3 / Wi-Fi |
| Ctrl+N / Ctrl+O / Ctrl+S | New / Open / Save |
| Ctrl+Shift+S | Save as |
| Ctrl+F / Ctrl+R / Ctrl+L | Find / Replace all / Go to line |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Ctrl+A / C / X / V | Select all / Copy / Cut / Paste |
| Ctrl+T | Statistics |
| Shift+arrows | Select |
| Ctrl+Left / Right | Word left / right |
| Ctrl+Up / Down | Start / end of document |
| Alt+Left / Right / Up / Down | Home / End / Page up / Page down |
| Home, End, PgUp, PgDn, Del, Ins | As labelled (Ins toggles overwrite) |
| Tab | Inserts 4 spaces |

## ePub export and the Xteink X3

**F7 – Export ePub** writes `name.epub` next to the document (untitled documents go to the current folder). You confirm the title, which defaults to the first `#` heading. The document is converted according to its type:

- **Markdown** (`.md`): headings, bold/italic/strikethrough, inline code, links, lists (nested, numbered, task boxes), block quotes, tables, fenced code and horizontal rules. `#` and `##` headings start new chapters and form the table of contents.
- **Plain text**: each line is a paragraph, which suits the editor's soft wrap.
- **Source code** (`.py`, `.cpp`, `.xml`...): monospaced lines with indentation kept.

Long chapters are split every ~48 KB so the reader never gets a huge XHTML file. The output is EPUB 2.0.1, which passes `epubcheck`. Author and book language are set in **Esc → ePub & X3 settings**.

**F8 – Send to X3** exports the book, then uploads it to the reader:

1. On the X3 (CrossPoint firmware), open **File Transfer** and choose **Join network**, so it joins the same Wi-Fi as the PicoCalc.
2. On the PicoCalc press **F8**. The first time, pick your network from the scan and type the password (Tab shows it).
3. The book lands in the X3 folder set in the settings (default `/`). If a book with the same name is already there, you are asked before it is replaced.

The X3 address defaults to `http://crosspoint.local/`, resolved with mDNS. If your network blocks mDNS, set the IP address the X3 shows on its screen instead. The last address that worked is also remembered as a fallback. **Test X3 connection** in the settings asks the reader for its status.

**F9 – Wi-Fi** shows the connection and lets you choose another network, turn the radio off, or forget the saved network. The radio is only switched on when needed and turns itself off after 10 idle minutes.

**Privacy note:** the Wi-Fi name and password are stored in plain text in `/.fwedit/wifi.txt` on the SD card, so anyone with the card can read them.

## Building

Requirements: Pico SDK 2.x, `arm-none-eabi-gcc`, CMake ≥ 3.13.

```sh
git clone https://github.com/raspberrypi/pico-sdk   # if you don't have it
cmake -B build -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build -j
```

FatFs R0.15 is downloaded automatically at configure time. To build offline, point `-DFATFS_DIR=` at a folder containing `ff.c`, `ff.h`, `ffunicode.c`, `diskio.h`.
The project defaults to `PICO_BOARD=pico2_w` (Wi-Fi is needed for F8/F9).
`lib/picocalc_ui` is part of this repository and is built automatically.

The result is `build/picocalc_writer.uf2`.

## Flashing

Hold **BOOTSEL** on the Pico while connecting USB and copy the `.uf2` onto the `RP2350` drive.
If you use a PicoCalc SD-card bootloader (e.g. the community UF2 loader), copy the `.uf2` into its firmware folder on the SD card instead.

The SD card must be FAT32 or exFAT. The editor creates a hidden `/.fwedit/` folder for its scratch files and settings.

## How it works

- **Shared UI library.** `lib/picocalc_ui` provides the LCD driver (`pcui::lcd`), the Terminus fonts (`pcui::gfx`) and the menus (`pcui::menu`, items made with `pcui::action`, `pcui::choice` and `pcui::range`). The library does not read the keyboard: `src/ui/dialogs.cpp` passes it Writer's keyboard through a small `pcui::Input` adapter, and converts the current theme with `dlg::pcuiTheme()`. Writer's own code keeps the editor-specific parts: the text renderer with per-character colours, the themes, and the prompt, confirm, message and file-browser dialogs. Keep `lib/picocalc_ui` identical in both projects; a git submodule is the easiest way.

- **Piece table on SD.** The opened file is never loaded into RAM: it is read in place, and everything you type is appended to `/.fwedit/add.tmp` (with a 4 KB RAM tail). The document is a list of 8-byte pieces pointing into those two files, backed by a 16 KB page cache.
- **Undo** stores piece-list snapshots of the changed region, not text, so undoing a huge paste or replace costs almost nothing.
- **Saving** writes a complete new file (`name.~fw`), then replaces the original and reloads it. It is crash-safe against partial writes, but it clears the undo history.
- **Layout** is computed on demand from the start of the current logical line, so there is no line index to build when opening a big file.
- **Rendering** uses a single-row line buffer with per-row hashing, so only rows that changed are resent to the LCD. There is no framebuffer.
- **Wi-Fi** uses the CYW43 driver with lwIP in poll mode: nothing runs in the background, and the network is pumped only by the main loop and while an operation waits. Uploads stream the ePub from the SD card straight into TCP as a `multipart/form-data` POST to CrossPoint's `/upload` endpoint.
- **Syntax colours** come from a small streaming lexer (`src/editor/syntax.cpp`) that only ever sees the visible rows. Everything that can span lines (block comments, docstrings, fenced code, multi-line XML tags) lives in a ~40-byte state that is cached at the top of the screen. That state stays valid while you type below it, and scrolling down continues from it. When it has to be rebuilt (jumping around, or editing above it) the lexer re-reads at most 8 KB before the top of the screen (`syn::kBackScan`). A construct opened further back than that, such as a code fence 8 KB above, may be coloured wrongly until you scroll closer.
- RAM use is about 107 KB static (most of it the Wi-Fi driver and lwIP buffers) plus the piece table; the firmware is about 530 KB of flash. After a very long editing session the table can fill up; the editor warns you to save, which compacts it.

## Start-up diagnostics

While starting, the firmware prints a short log (version, reset reason, and each step: display, keyboard, SD card, scratch files, settings, last file). The last step reached is also kept in a watchdog scratch register, so if a start is interrupted by a reset, the next start shows `previous start stopped at: ...`. If that step was reopening the last file, the editor skips it once (safe start). HardFaults and panics show a red crash screen with the program counter instead of locking up silently.

## Notes and limits

- New lines are inserted as LF; existing CRLF files keep their CRLF line endings. Tabs in existing files are shown as one cell.
- Maximum document size is 2 GB.
- Tunables in `src/hw/board.h`: `LCD_SPI_HZ` (25 MHz, as in the ClockworkPi driver; the panel is driven in its 18-bit serial colour mode), `LCD_INVERT` (set to 0 if colours look inverted), `KBD_READ_DELAY_US` (16 ms by default, like the ClockworkPi examples), `SD_SPI_HZ`.
- The core editing engine and the syntax lexer are tested on a PC (`test/`, `make` runs a randomized fuzz test with ASan/UBSan against a reference string, plus the lexer tests). `make` also converts sample documents to ePub and checks them (epubcheck is used if it is in `/opt/epubcheck-*`). It then uploads one to `mock_crosspoint.py`, a stand-in for CrossPoint's file-transfer server. `test/appsim/build.sh` builds a host simulator of the whole app that renders frames to `out/*.ppm`. Versions 1.0, 1.1 and 2.0 (including ePub export and sending to the X3) run on real PicoCalc hardware. The 2.1 menus were checked in the simulator, and the editor screens it renders are pixel-identical to 2.0's.

## Licences

- Terminus font: SIL Open Font License 1.1 (converted to `lib/picocalc_ui/src/fonts_terminus.h` with `tools/gen_fonts.py`)
- FatFs: ChaN's BSD-style licence
