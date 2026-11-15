# REPO_MAP: what we touch, what we never touch

Upstream base: `develop` @ `622e5de9` (13 Nov 2026). Our work: branch `ours`. Paths are relative to `firmware/`. Anything not read in the source is marked **unverified**.

## 1. Where a theme lives

A theme is two things: a `BaseTheme` subclass (drawing) and a `ThemeMetrics` constant (plain-data geometry).

- Enum: `UI_THEME { CLASSIC, LYRA, LYRA_3_COVERS, ROUNDEDRAFF, COVER_GRID }` at `src/CrossPointSettings.h:206`. Default `LYRA` at `:356`.
- Registration: `UITheme::setTheme()` switch at `src/components/UITheme.cpp:39-73`. One `case` sets `currentTheme` (the class) and `currentMetrics` (the metrics).
- Settings entry: `homeThemeValues()` at `src/SettingsList.h:278-282`, used at `src/SettingsList.cpp:52`. Display names are `STR_THEME_*` ids; strings live in `lib/I18n/translations/*.yaml` and generate `I18nKeys.h` (`scripts/gen_i18n.py`).
- Drawing hooks: virtuals in `src/components/themes/BaseTheme.h:232-309` (`drawHeader`, `drawButtonHints`, `drawButtonMenu`, `drawRecentBookCover`, `drawPopup`, `drawTextField`, `drawSubHeader`, battery). Access macro: `GUI` (`UITheme.h:64`).
- Metrics: `ThemeMetrics` struct at `BaseTheme.h:30-132`; Classic defaults at `:156-230`; Lyra values at `themes/lyra/LyraTheme.h:8-85`.
- `src/components/UIThemeTokens.h:13-45` copies metrics into FreeInkUI `ThemeTokens` (row gap, radius, selection style, header, control radii). **Adding a theme needs no edit here**: it reads whatever `getMetrics()` returns. Shape comes from metrics, sizes from the uiScale fonts (`src/components/UIScale.h:14-22`, `UI_10_FONT_ID`/`UI_12_FONT_ID`).
- `ThemeTokens` (`freeink-sdk/libs/ui/FreeInkUI/include/FreeInkUICore.h:638-694`) has more fields than metrics forward (spacing, `listSeparator`, per-state `StyleSet`s). Reaching them means editing `UIThemeTokens.h` (upstream file, small merge risk).

## 2. What a theme controls, what it does not

| Surface | Controlled by theme? | Evidence |
|---|---|---|
| Header band, button hints, popups, text field, battery | Yes, via `GUI.*` | `GUI.drawHeader` in `UiListActivity.cpp:241`; ~50 activity files call `GUI.*` |
| Home menu and recent-book tile | Yes, theme-driven and hand-rolled (legacy bridge) | `HomeActivity.cpp:535-579`; `docs/contributing/touch-and-ui.md:7` |
| Lists, tabs, settings rows (FUI screens) | Shape only: gaps, radii, selection style, scroll bar, header | `UIThemeTokens.h:20-44` |
| Reader page text and layout | No. Typography comes from settings and `lib/Epub`, not the theme | `EpubReaderActivity.cpp` |
| Reader footer status bar | Called as `GUI.drawStatusBar` (`EpubReaderActivity.cpp:2511`) but the method is **static, not virtual** (`BaseTheme.h:293`): a theme cannot override it without editing `BaseTheme` |
| Cover-grid home | Separate path, needs PSRAM, so S3 only | `UITheme.cpp:33-37`, `.agents/rules/ui-activities.md:30` |
| Web page at `crosspoint.local` | No. HTML under `src/network/html/` (generated to `*.generated.h`) | `docs/contributing/architecture.md` |
| Colors | Not applicable: 1-bit plus optional 4-level gray | `hardware-resources.md` |

## 3. We touch (our files)

- New: `src/components/themes/<ours>/<Ours>Theme.{h,cpp}` (class plus `<Ours>Metrics`), `docs/ours/*`.
- Small edits to upstream files, unavoidable and one-line each: `CrossPointSettings.h:206` (enum), `UITheme.cpp:39-73` (case plus include), `SettingsList.h:278-282` (list), `lib/I18n/translations/english.yaml` (name string). Keep each edit minimal and at the end of its list.
- Later, for screens the theme cannot reach: our own `Activity` subclasses, not edits to `HomeActivity`/reader.

## 4. We never touch

- `freeink-sdk/` (submodule, hardware layer). Pin the revision, do not edit.
- `lib/hal/`, board profiles, `platformio.ini` env flags, `board_upload.offset_address = 0x10000` (`platformio.ini:36`).
- `src/network/` and `docs/webserver-endpoints.md` (see 6).
- Generated files: `lib/EpdFont/builtinFonts/*`, `src/network/html/**/*.generated.h`, `lib/I18n/I18nKeys.h`, hyphenation tries.
- `lib/Epub`, cache formats (`docs/file-formats.md`), reader internals.

## 5. Fonts, Cyrillic, screenshots

- Built-ins: Literata 12/14/17 px x-height (reader) and Ubuntu 8/10/12 (UI), registered at `src/main.cpp:78-97` and `:529-530`; ids in `src/fontIds.h`.
- **Cyrillic is in the built-ins.** `fontconvert.py:87-89` exports U+0400-04FF. Noto fallback glyphs provide missing Literata coverage, while the Ubuntu headers also carry the range (`{0x400, 0x4FF, ...}`). That range holds ґ є і ї (U+0490-0491, 0404, 0456, 0457). Checked on three font files only; **the remaining weights are unverified**. Whether glyphs render correctly, and hyphenation for ru/uk, needs the device.
- SD-card fonts (`.cpfont`, TTF/OTF on PSRAM boards only): `docs/sd-card-fonts.md`.
- Screenshots: power + down buttons (`src/main.cpp:921-930`, also `EpubReaderActivity.cpp:1989`) write a 1-bit BMP to `/screenshots/` on the SD card (`ScreenshotUtil.cpp:81-203`). **Needs a device with an SD card; there is no host rendering path.**
- Without a device: the SDK already has a host renderer. `freeink-sdk/libs/ui/FreeInkUI/tools/builder/server.py` serves a local page (`127.0.0.1:8088`) that compiles a screen against the real `DisplayTarget` and returns SVG (`freeink-sdk/docs/freeink-ui.md:352-366`). `tools/render_gallery.sh` does the same for the component gallery into `docs/images/`. Both use the SDK's own compact font and default tokens (`render_gallery.cpp:7-10`), **not our theme metrics or the firmware fonts**. A harness that feeds our `ThemeMetrics` and the Ubuntu UI font into the same renderer is small new work (**unverified, not tried**). The firmware-side `FreeInkUI` host tests (`test/host/run.sh`) only check, they do not draw.

## 6. Frozen API surface (app compatibility)

Do not remove or rename; new optional fields are fine.

- Ports: HTTP 80, WebSocket 81 (`src/network/CrossPointWebServer.h:82-83`), UDP discovery 8134 (`CrossPointWebServer.cpp:125`), WebDAV on 80 (`:447`).
- Discovery reply format `crosspoint (on <hostname>);<wsPort>` to a `hello` datagram (`:636-655`).
- mDNS hostname `crosspoint` (`src/activities/network/CrossPointWebServerActivity.cpp:48`).
- `GET /api/status` fields: `version`, `firmwareFamily`, `pluginApi`, `pluginCapabilities`, `ip`, `mode`, `rssi`, `freeHeap`, `uptime`, `device`, `serial`, optional `deviceId` (`CrossPointWebServer.cpp:~715-780`).
- `firmwareFamily = "crosspoint"`: `src/util/FirmwareIdentity.h:5`.
- **Known difference:** our hotspot is the open network `wren-xxxx` (`src/wren/send/SendState.h:19`), not
  `CrossPoint-Reader` as `docs/webserver-endpoints.md:550` says. Kept on purpose (Daniil, 15 Nov). Ports, mDNS and
  UDP discovery are unchanged. Smoke test: Send to X4 and CrossPoint Sync must also work over the hotspot.
- File API, upload, WebSocket upload, settings, fonts, OPDS, Wi-Fi, plugin endpoints: `docs/webserver-endpoints.md` (HTTP pages `:14-28`, status `:31-68`, file API `:70-226`).
- **Side effect to watch:** `uiTheme` is a key in `/api/settings` (`SettingsList.cpp:52`). A new enum value is additive, but do not reorder existing values.
- `device` value: `"X3"` or `"X4"` on the C3 binary (`gpio.deviceIsX3()`, `:745-747`); on other envs it returns `BoardConfig::ACTIVE.name`. The exact string for X4 Classic and X4 Pro is **unverified** (not looked up in the SDK profiles).
