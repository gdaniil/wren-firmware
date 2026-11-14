# Wren X3 web flasher

A static Web Serial installer for Chrome or Edge on a computer. No package
installation or web build step is required. Nothing is sent to a remote service.

## Local use

Build the app with `pio run -e default`. Copy this page, `app.js`, `styles.css`,
`lib/`, and `.pio/build/default/firmware.bin` into a temporary serving folder.
Optionally add `firmware.json` with `sha`, `short_sha`, `built_at` (ISO UTC), and
`env: "default"`. In that folder run `python3 -m http.server 8000`, then visit
`http://localhost:8000`. Web Serial needs localhost or HTTPS.

Connect an awake X3 using the 4-pin magnetic cable. Save the downloaded full
16 MB backup somewhere safe, or confirm you already have a backup of this X3.
Install Wren, wait for completion, unplug, and press power to restart.
The browser can initiate the backup download but cannot confirm that the user
kept it. Keep the page open during backup and installation.

## Flash contract

The reader must identify as ESP32-C3 with 16 MB flash. Secure boot and active
flash encryption are checked using read-only registers `0x60008838` (bit 20)
and `0x60008834` (odd parity of bits 18–20), respectively; unreadable registers
stop the connection. Definitions are from
[Espressif's C3 target](https://github.com/espressif/esptool/blob/master/esptool/targets/esp32c3.py).
The exact reference X3 partition layout is required; other layouts stop safely.
Chip/flash checks alone cannot distinguish every C3 board; select only an X3.

Reads: partition table at `0x8000` (4096 bytes), otadata at `0xe000` (8192 bytes),
and full flash `0x0` through `0xffffff` for backup in 64 KiB chunks.
Each backup chunk is checked against the device MD5 before it is accepted.
The backup uses 16 MB of browser memory plus download/transport copies, not
reader heap. Chunking bounds esptool's repeated concatenation during reads.

Writes follow the reference CrossPoint-family X3 OTA sequence:

- Inactive app at **`0x10000` or `0x780000`**, at most `0x770000` bytes.
- Then one **4096-byte** otadata sector at **`0xe000` or `0xf000`**.
  The sector is chosen independently of the app index; the other sector survives.
  The new sequence, NEW state and CRC select the installed app. The selector is
  read back and verified before success is shown.

All writes use `eraseAll: false`; the library erases the sectors being written.
No full erase, bootloader, partition table, NVS, or filesystem write is requested.
Image segment/checksum validation, SHA validation when a trailer is present,
and an exact C3 image ID check happen
before writing. Written app and selector contents are also verified by MD5
before success; app verification precedes the selector write. SHA detects corruption, not publisher authenticity; HTTPS and
repository deployment permissions establish the source. No serial reset follows
installation. Interrupted app writes leave the old boot selector unchanged;
a selector verification failure is reported and requires reconnecting.

## Checks and deployment

Run `node --test web-flasher/test/` (Node 22 or newer). Tests use fake loaders;
they never request a serial port. Hardware checks belong to the human: preserve
a backup, confirm wrong hardware/layout fails, then verify installation, manual
restart and reading on an X3. A backup may take several minutes.

`wren-pages.yml` builds `default`, tests this code, and uploads the static site
with its firmware and build metadata on pushes to `ours` except docs-only changes.
Enable GitHub Pages with **GitHub Actions** as its source in repository settings.
The workflow deploys after being pushed; no deployment is performed by local edits.

Third-party attribution and the reference commit are in `LICENSE-THIRD-PARTY.md`.
