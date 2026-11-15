# System (service) screens: work plan

Source: design canvas "Wren design", page "Wren misc" (title "Wren · system"): Sleep, Sleep · no book, Boot,
No card, Busy plate, Battery empty, Crash, Update · installing, and the page brief. Copies:
`docs/ours/design/system/`. Written 15 Nov 2026 on `ours` (uncommitted library/setup/send work in the tree).

## Decisions

1. **Busy plate = a Wren theme.** `WrenTheme` derives from `LyraTheme` and overrides `drawPopup` and
   `fillPopupProgress`. `UITheme::setTheme` uses it for `LYRA` (the default) and `COVER_GRID`. No new enum
   value, so `/api/settings` `uiTheme` is unchanged. Our own screens call `wren::ui::drawBusyPlate` directly
   (pose, headline, label).
2. **Sleep.** `renderDefaultSleepScreen` draws Wren for `DARK` and `LIGHT` (and every fallback that lands
   there). All `SLEEP_SCREEN_MODE` values stay. The "going to sleep" plate is skipped on the Wren path
   (proposed in the design note). Setup shows the option as `wren · cover`.
3. **Percent only on the sleep card**, no "p. 3/612": the same decision as the library card.
4. **No card** replaces the boot-time "SD card error" message. Select restarts the reader (setup() returned
   early, so a restart is the honest remount). Runtime card loss in library/files is not changed here.
5. **Crash:** Wren layout, select or back goes home.
6. **Not built now:**
   - Update · installing: dropped (Daniil, 15 Nov). Updates are manual: merge upstream by hand about once a
     month, release our own builds through the web flasher. The upstream "Check for updates" row is removed
     from `SettingsActivity.cpp` (it would install upstream CrossPoint over Wren). SD-card firmware update
     (recovery: button held at boot) stays as the manual path.
   - Battery empty: the design asks to measure the BQ27220 reading near cut-off first. A wrong threshold at
     boot could keep the reader from starting.

## Tasks

| # | Task | Files | Who |
|---|---|---|---|
| Y0 | Sprites (WrenSleeping, WrenHello, WrenNoCard, WrenConfused, WrenLowBattery, WakeArrow), strings ru/uk/en, i18n regen | `scripts/wren/build_icons.py`, yaml | Claude, done |
| Y1 | Sleep + Sleep · no book | `src/wren/system/WrenSleepScreen.*`, `SleepActivity.cpp`, setup display labels, book info helper | Codex Terra high |
| Y2 | Busy plate (`WrenTheme`, `drawBusyPlate`) | `src/wren/ui/WrenBusyPlate.*`, `src/wren/ui/WrenTheme.*`, `UITheme.cpp` | Codex Terra high |
| Y3 | Boot, No card, Crash | `src/wren/system/WrenSystemScreen.*`, `WrenNoCardActivity.*`, `BootActivity.cpp`, `CrashActivity.cpp`, `main.cpp` | Codex Terra high |

Briefs: `briefs/Y1_SLEEP.md`, `briefs/Y2_BUSY_PLATE.md`, `briefs/Y3_SYSTEM_SCREENS.md`. Y1–Y3 touch separate
files and run in parallel.

## Status

- Y0 done (15 Nov).
- Y1–Y3 done by Codex Terra high (15 Nov), reviewed. Fixed after review: the sleep option order moved into
  `SetupState::sleepScreenIndex` (Codex had flipped indexes in the activity), the transparent-sleep band in
  `drawSleepPopupPreservingFrame` now covers the taller plate (`busyPlateBounds`), unused `thumbPath`
  declaration removed from the home header.
- Checks: `pio run -e default|x4c|x4pro` pass, `clang-format-fix -g` clean, `pio check` clean, unit tests
  992/992. RAM static 66,664 B (default). Hardware not tested.

## Known limits

- The sleep card crops the library's cached thumbnail (84 or 196 px tall) to 40x60 instead of scaling it.
- No card shows in English: the saved language is applied later in setup().
- Crash text names `crash_report.txt` even if writing the report failed.
- "Place is saved" holds for EPUB (progress written on every rendered page, `EpubReaderActivity.cpp:2018`);
  TXT and XTC not checked.
- The plate's progress bar assumes a one-line headline; no current caller passes a long one.
- Not built: page N saved / opening <title> labels (callers do not pass them), errors staying 2 s.
