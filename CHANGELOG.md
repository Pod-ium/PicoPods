# Changelog

## Unreleased

### Libraries
- Host terminal: interactive SDL2 window for every pod (screen, live
  keyboard, sound; F12 saves a screenshot). Opens when `PD_KEYS` is not set;
  `PD_WINDOW`, `PD_SCALE`; built when SDL2 is found (`-DPD_HOST_SDL`).
  Headless runs and the tests are unchanged.

- PicoPods Web: the pods in a browser (`tools/build_web.sh`, `web/`), with
  a touch PicoCalc keyboard, sound, an IndexedDB SD card and pod files that
  load like cartridges. `pd::sys` yields to the page from its sleeps and
  clock (Asyncify); `pd_net` has no Wi-Fi there.

### Pods
- **PodBoy 1.2**: display SPI at 50 MHz instead of 25 MHz, so scrolling
  scenes (every line changes) reach the screen at ~20 fps instead of ~11
  (#12). The other pods keep the conservative 25 MHz.
- **PicoCraft 1.8** (new pod): the voxel sandbox game PicoCraft 1.7 moved
  onto the libraries. 1.7 did not work on the PicoCalc: its own display
  driver set 16-bit colour, which the panel ignores over SPI (it only takes
  18-bit), toggled CS/DC with no settle time and split the window setup
  from the pixels; its keyboard reads waited 150 µs instead of the 16 ms
  the controller needs. The frame now goes to the panel in 18-bit bands by
  DMA while both cores ray-cast the next one (rows handed out one at a
  time), and the keyboard is read while core 1 renders. Saves of 1.7 still
  load. Needs an RP2350.
- Pod Writer, Pod Files: `main()` ends with `return 0` (needed now that the
  host window renames it).

### Build and CI
- G-host installs `libsdl2-dev`, so the window code is compiled on every push.
- G-web builds PicoPods Web on every push (artifact `picopods-web`);
  `pages.yml` publishes it to GitHub Pages once enabled (`PICOPODS_PAGES`).

## PicoPods 1.0.0 (2026-10-24)

First release of the PicoPods tree: the common PicoCalc libraries and three
pods, assembled from PicoCalc Writer 2.1.0, PicoBoy 1.0 and picodev 1–2
(see `docs/ROADMAP.md` for how, and `docs/MIGRATION.md` for the file map).

### Libraries
- picodev 2 renamed PicoPods: `cmake/picopods_init.cmake`,
  `cmake/picopods.cmake`, `picopods_add_pod()`. Targets (`pd_*`),
  headers (`pd/...`), the `pd::` namespace and `PD_*` options unchanged.
- `lib/` grouped by layer: `hardware/`, `services/`, `interface/`,
  `content/` (catalogue in `lib/README.md`).
- `pd::epub::Options::generator` (OPF generator metadata, default
  "PicoPods"); it was fixed to "PicoCalc Writer".
- Outward-facing identifiers say PicoPods: HTTP/Git user agent,
  USB drive vendor/manufacturer, default Bluetooth key file
  `/.picopods/bt_keys.bin`.

- UF2 Loader on RP2040: `pd_flash_cache` used the whole flash and could
  overwrite the loader in its top 16 KB (the demo's flash test on a Pico /
  Pico W). It now honours the loader's marker in the app's vector table and
  leaves the top 16 KB free even without it (`pd/uf2loader.h`, host-tested).
  The RP2350 path already stayed inside the app partition.

### Pods
- **PodBoy 1.1** (was PicoBoy 1.1): `pods/podboy`, `podboy.uf2`. Rename only.
- **Pod Writer 3.0.0** (was PicoCalc Writer 3.0.0): `pods/podwriter`,
  `podwriter.uf2`, Wi-Fi host name `PodWriter`, ePub generator
  "Pod Writer". Data stays in `/.fwedit/`.
- **Pod Files 1.3.0** (was PicoCalc Files 1.2.11): `pods/podfiles`,
  `podfiles.uf2`. Work folder `/.podfiles/`; an existing
  `/.picocalc_files/` is renamed on first start.

### Examples
- `pd_demo`, `pd_template`, `pd_usbprobe` moved to `examples/`, with
  PicoPods names; the demo's Wi-Fi file is `/.picopods/wifi.txt`.

### Build and CI
- `tools/gate.sh`: the merge gates (host build + tests, firmware build
  per board) in one command.
- GitHub Actions run both gates on every push and pull request and keep
  the `.uf2` files of each board as artifacts.
- Prebuilt `.uf2` files are no longer committed; take them from CI.
- Release workflow: every published release gets `podboy_<version>.uf2`,
  `podwriter_<version>.uf2`, `podfiles_<version>.uf2` and a SHA256SUMS file
  as release assets (`tools/package_release.sh` builds them locally too).
