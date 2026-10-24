# Changelog

## PicoPods 1.0.0 (unreleased)

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
