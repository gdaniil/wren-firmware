# Y3: Boot, No card, Crash (Wren look)

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours` (uncommitted Wren work is in the tree; keep it).
ESP32-C3 baseline: ~380 KB RAM, no PSRAM.

## Constraints (hard)

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`, generated files (`WrenIcons.generated.h`, `I18nKeys.h`, fonts). Do not edit yaml translations: the
strings below already exist. Do not run builds or tests; I run them. Other agents are editing at the same time:
touch ONLY the files listed under "Files". Do not touch `test/wren_layout/CMakeLists.txt`, `WrenChrome.*`,
`UITheme.cpp`, `SleepActivity.cpp`, `src/wren/home/`, `src/wren/system/WrenSleepScreen.*`.

## Files

- New: `src/wren/system/WrenSystemScreen.h`, `src/wren/system/WrenSystemScreen.cpp` (shared layout).
- New: `src/wren/system/WrenNoCardActivity.h`, `src/wren/system/WrenNoCardActivity.cpp`.
- Edit (minimal): `src/activities/boot_sleep/BootActivity.cpp`, `src/activities/home/CrashActivity.cpp`
  (and its `.h` only if needed), `src/main.cpp` (only the SD failure branch at `:613-618`).

## Shared layout (`WrenSystemScreen`)

Designs `docs/ours/design/system/NoCard.html` and `Crash.html` (528x792 portrait, 1-bit), same layout as
"Card unreadable". One function, e.g. `drawStateScreen(renderer, const Icon& sprite, headline, body, extra)`:

- Status bar on top: `wren::ui::drawMenuStatusBar(renderer, clock, battery)` exactly as
  `WrenHomeActivity` does (`WrenHomeActivity.cpp:592`, copy its clock formatting).
- Block left-aligned at 20 px side padding, vertically centred in the space between the status bar and the
  bottom tab strip (design: `padding-bottom` 80 px no card, 64 px crash, i.e. the block sits a bit above
  centre): sprite at scale 4 (128 px), 24 px gap, headline in Golos 56 headline font (line 53 px, wrap to 2
  lines max if needed), 14 px gap, body in Golos 18 wrapped (line 25 px).
- Optional extra block for Crash (below).
- Bottom: `drawButtonTabs` with only tab 2 (select position) filled.
- Must work in all four orientations: in landscape shrink body lines (`drawTextWrapped` maxLines) before
  overlapping the tabs.

## 1. Boot (`BootActivity::onEnter`, `BootActivity.cpp:10-21`), design `Boot.html`

Clear, then centred group (horizontally and vertically): `icons::WrenHello` at scale 6 (192 px), 24 px gap,
`STR_WREN_BOOT_HELLO` in Golos 56 headline font centred, 12 px gap, label `fw 0.1.0` (format
`STR_WREN_FW_VERSION` with `WREN_VERSION` from `src/wren/WrenVersion.h`) centred. Nothing else. One
`renderer.displayBuffer()` as today. Remove the CrossPoint logo and texts from this screen.

## 2. No card, design `NoCard.html`

`WrenNoCardActivity`: sprite `icons::WrenNoCard`, headline `STR_WREN_NOCARD_TITLE`, body
`STR_WREN_NOCARD_BODY`, tab 2 `STR_WREN_TAB_SELECT`. Select (Confirm) → `ESP.restart()` (setup() returned early
on the failure, so a restart is the remount). No other button does anything. In `main.cpp:613-618` replace
`goToFullScreenMessage("SD card error", ...)` with this activity (via `activityManager.replaceActivity`). Check
that `loop()` still runs input and rendering for an activity when `setup()` returned early there; if it does not,
report it rather than restructuring `main.cpp`. Expose the drawing as a function too so library/files can reuse
it later (do not wire it into them now). Note: on the boot path the clock may be unset; draw what
`drawMenuStatusBar` gets.

## 3. Crash (`CrashActivity`), design `Crash.html`

Replace `render` with the shared layout: sprite `icons::WrenConfused`, headline `STR_WREN_CRASH_TITLE`, body
`STR_WREN_CRASH_BODY`. Extra block, 28 px under the body: 3 px ink rule, 12 px gap, label
`STR_WREN_CRASH_REASON`, 8 px gap, the panic text in mono 16 (`WREN_MONO_16_FONT_ID`, NOT uppercased: draw it
with `drawTextWrapped`, not `drawLabel`), max 4 lines, ellipsis. No reason recorded (`getPanicInfo` empty):
hide the whole block (do not show `STR_CRASH_NO_REASON`). Tab 2 `STR_WREN_SYS_HOME`. Input: Confirm or Back →
`finish()` (today only Back). Keep the tap handling. Keep `STR_CRASH_*` keys untouched in yaml.

## Fonts and icons

Wren fonts: `src/wren/ui/WrenFonts.h` (font ids). Helpers: `src/wren/ui/WrenChrome.h` (`drawTextFit`,
`drawTextWrapped`, `drawLabel`, `drawIcon`, `drawButtonTabs`, `drawMenuStatusBar`). Icons:
`src/wren/ui/WrenIcons.generated.h`. Look at `WrenHomeActivity::drawEmpty` for how an existing Wren state screen
is drawn.

## Done

List every file changed, anything not done, anything you are unsure of.
