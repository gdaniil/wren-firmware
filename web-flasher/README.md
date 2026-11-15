# Wren web flasher

A static Web Serial installer for Chrome or Edge on a computer. No package
installation or web build step is required. Nothing is sent to a remote service.

## Local use

Build with `pio run -e default -e x4c -e x4pro`. Copy this page, `app.js`,
`styles.css`, `lib/`, and the three build outputs into a temporary serving folder:
`default/firmware.bin` as `firmware.bin`, `x4c/firmware.bin` as
`firmware-x4c.bin`, and `x4pro/firmware.bin` as `firmware-x4pro.bin`.
The build outputs are under `.pio/build/`.
Optionally add `firmware.json` with `sha`, `short_sha`, `built_at` (ISO UTC), and
`envs: ["default", "x4c", "x4pro"]`. In that folder run `python3 -m http.server 8000`, then visit
`http://localhost:8000`. Web Serial needs localhost or HTTPS.

Choose X3 or X4 (C3), X4 Classic, or X4 Pro and connect an awake reader with
a data cable. X4 Classic and X4 Pro are untested on hardware. Save the full
16 MB backup somewhere safe, or confirm you already have a backup of this reader.
Install Wren, wait for completion, unplug, and press power to restart.
The browser can initiate the backup download but cannot confirm that the user
kept it. Keep the page open during backup and installation.

## Flash contract

The selected model must identify as ESP32-C3 (X3 or X4) or ESP32-S3
(X4 Classic or X4 Pro), with 16 MB flash. Secure boot (bit 20) and active
flash encryption (odd parity of bits 18–20) are checked with read-only eFuse
registers. C3 uses `0x60008838` and `0x60008834`; S3 uses `0x60007038`
and `0x60007034`. Unreadable registers stop the connection. Sources:
[Espressif C3](https://github.com/espressif/esptool/blob/master/esptool/targets/esp32c3.py)
and [S3 targets](https://github.com/espressif/esptool/blob/master/esptool/targets/esp32s3.py).
The exact selected partition layout is required. Chip and layout checks cannot
identify every compatible-looking board or distinguish X4 Classic from X4 Pro;
choose your actual reader.

S3 layout (offset / size): NVS `0x9000 / 0x5000`, otadata `0xe000 / 0x2000`,
app0 `0x10000 / 0x7e0000`, app1 `0x7f0000 / 0x7e0000`, SPIFFS
`0xfd0000 / 0x14000`, coredump `0xfe4000 / 0x1c000`. Sources:
[SDK X4 Pro](https://github.com/Free-Ink/freeink-sdk/blob/6a208c0c46cce99bafa334bc4f171f5e558c5195/docs/xteink-x4pro-support.md) and
[SDK X4 Classic](https://github.com/Free-Ink/freeink-sdk/blob/6a208c0c46cce99bafa334bc4f171f5e558c5195/docs/xteink-x4c-support.md).

Reads: partition table at `0x8000` (4096 bytes), otadata at `0xe000` (8192 bytes),
and full flash `0x0` through `0xffffff` for backup in 64 KiB chunks.
Each backup chunk is checked against the device MD5 before it is accepted.
The backup uses 16 MB of browser memory plus download/transport copies, not
reader heap. Chunking bounds esptool's repeated concatenation during reads.

Writes follow the reference CrossPoint-family OTA sequence:

- Inactive C3 app at **`0x10000` or `0x780000`**, at most `0x770000` bytes;
  S3 app at **`0x10000` or `0x7f0000`**, at most `0x7e0000` bytes.
- Then one **4096-byte** otadata sector at **`0xe000` or `0xf000`**.
  The sector is chosen independently of the app index; the other sector survives.
  The new sequence, NEW state and CRC select the installed app. The selector is
  read back and verified before success is shown.

All writes use `eraseAll: false`; the library erases the sectors being written.
No full erase, bootloader, partition table, NVS, or filesystem write is requested.
Image segment/checksum validation, SHA validation when a trailer is present,
and an exact selected-chip image ID check happen
before writing. Written app and selector contents are also verified by MD5
before success; app verification precedes the selector write. SHA detects corruption, not publisher authenticity; HTTPS and
repository deployment permissions establish the source. No serial reset follows
installation. Interrupted app writes leave the old boot selector unchanged;
a selector verification failure is reported and requires reconnecting.

## Checks and deployment

Run `node --test web-flasher/test/` (Node 22 or newer). Tests use fake loaders;
they never request a serial port. Hardware checks belong to the human: preserve
a backup, confirm wrong hardware/layout fails, then verify installation, manual
restart and reading on each selected model. A backup may take several minutes.

`wren-pages.yml` builds `default`, `x4c`, and `x4pro`, tests this code, and uploads the static site
with its firmware and build metadata on pushes to `ours` except docs-only changes.
It also builds the reading fonts (`scripts/wren/build_fonts.py --weights measured`, cached on the
font scripts) and publishes them as `wren-fonts.zip` with `fonts/<Family>/` at the archive root,
ready to copy to the SD card. `firmware.json` then carries `fonts_bytes` for the link label.

The combined firmware is GPLv3 (wolfSSL; see `licenses/README.md`). The workflow runs
`scripts/build_release_license.py` for each env and publishes `LICENSE-firmware.txt`,
`LICENSE-firmware-x4c.txt` and `LICENSE-firmware-x4pro.txt` beside the binaries, plus
`LICENSE-Apache-2.0.txt` for the vendored esptool-js. `firmware.json` carries `source_url`, the
exact source tree of the published build. The page footer links all of them, so the repository
must stay public while this site serves its binaries.
Enable GitHub Pages with **GitHub Actions** as its source in repository settings.
The workflow deploys after being pushed; no deployment is performed by local edits.

Third-party attribution and the reference commit are in `LICENSE-THIRD-PARTY.md`.
