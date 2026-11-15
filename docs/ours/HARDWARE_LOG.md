# Hardware log

What ran on which reader. One entry per test session. We own one reader: an Xteink X3.

| Date | Reader | Firmware | Result | Tested by |
| --- | --- | --- | --- | --- |
| 15 Nov 2026 | Xteink X3 | Wren, branch `ours` (15 Nov 2026) | Works | Daniil |

## Not tested on hardware

- Xteink X4 (ESP32-C3): builds, never run.
- Xteink X4 Classic (ESP32-S3): builds, never run.
- Xteink X4 Pro (ESP32-S3): builds, never run.

## Per release

Before each release, test on the X3 and add a row:

- Install with the web installer, restart, open a book.
- Read an EPUB in English and one in Ukrainian (Cyrillic glyphs, hyphenation).
- Library, settings, end-of-book screen.
- Send a book over Wi-Fi and over the hotspot: built-in web page, Send to X4,
  CrossPoint Sync.
- Install CrossPoint back.
