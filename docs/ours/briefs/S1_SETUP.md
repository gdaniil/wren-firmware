# S1: Setup tab (root, display, system)

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours` (uncommitted library work in the tree is ours: keep it).
C++ (gnu++2a), PlatformIO + Arduino, ESP32-C3 baseline (~380 KB RAM, keep >50 KB free, no leaks).

Design (source of truth for geometry and words), 528x792 portrait, 1-bit (#151515 = ink, #F3F1EA = paper):
`docs/ours/design/setup/Setup.html`, `SetupDisplay.html`, `SetupSystem.html`. Ignore fonts in the HTML; use the
Wren fonts in `src/wren/ui/WrenFonts.h` (Golos 40 = `WREN_GOLOS_40_FONT_ID`, Golos 24, Mono 16 labels).

Patterns to copy: `src/wren/reader/WrenReaderSettingsActivity.{h,cpp}` (rows with `Marker::Options`, side buttons
change values, save on exit), `src/wren/home/WrenHomeActivity.cpp` (`drawHeader()` at ~line 360: Wren icon + three
tabs; tab switching in `loop()` ~line 780). Drawing helpers: `src/wren/ui/WrenChrome.{h,cpp}`, `WrenLayout.h`.

## Constraints (hard)

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`. Generated files: only `lib/I18n/I18nKeys.h` via `scripts/gen_i18n.py` after adding strings. New code
in new files under `src/wren/setup/`. Upstream files: only the one routing change below. Do not run builds or
tests and report them; I run them. Every heap allocation justified in a comment if not obvious.

## 1. Chrome additions (`WrenChrome`)

- `Marker::Toggle` + `Row::on` for `drawRow`: 48x26 box, 3 px border, 3 px padding, 14x14 knob. On: box filled,
  knob paper, right. Off: box empty, knob ink, left. On a selected (inverted) row swap ink/paper.
- Move the home header (Wren icon + library · send · setup tabs) into `WrenChrome` as
  `drawTabHeader(renderer, y, activeTab)` and make `WrenHomeActivity::drawHeader()` call it (active 0). Setup uses
  active 2. Keep home's look byte-identical.

## 2. `WrenSetupActivity` (design `Setup.html`)

Status bar (`drawMenuStatusBar`), tab header with setup active, then four big rows: Golos 40, 20 px side padding,
24 px top/bottom padding, 3 px ink divider, chevron right (`icons::ChevronRight` 2x). Selected row = ink fill, paper
text. Rows and targets:

| row | opens |
|---|---|
| reader | `WrenReaderSettingsActivity` with crumb `setup` (global settings, same screen as in the reader). It rotates the renderer when orientation changes; when it returns, restore the UI orientation the way home/other non-reader activities set it. |
| display | `WrenSetupDisplayActivity` |
| system | `WrenSetupSystemActivity` |
| files | placeholder for S2: open upstream `FileBrowserActivity` at `/` for now |

Input: front ▲▼ move (wrap, hold repeats), select opens, back → `activityManager.goHome()`. Side buttons switch
tabs like home: Up → send (`goToFileTransfer()`), Down → library (`goHome()`). Bottom tabs: back · select · ▲ · ▼,
plus `drawEdgeTabs(true, true)`.

Routing: `ActivityManager::goToSettings(section)` opens `WrenSetupActivity` when called with the default section;
an explicit section (e.g. `SettingsSection::Network` from `main.cpp:784`) keeps the upstream screen.

## 3. `WrenSetupDisplayActivity` (design `SetupDisplay.html`)

`drawHeader(renderer, 28, "setup", "display")`, then `drawRow` rows (same row component as reader settings).
▲▼ pick a row, side buttons and select change the value, back saves (`SETTINGS.saveToFile()` only if changed) and
returns.

| label | control | setting |
|---|---|---|
| sleep screen | options: cover, dark | `sleepScreen`: COVER / DARK. Any other stored value shows no square chosen and the first change sets COVER. |
| quick resume | toggle | `quickResumeSleepScreen` |
| battery % | options: show, hide | `hideBatteryPercentage`: show = HIDE_NEVER, hide = HIDE_ALWAYS (IN_READER shows as hide) |
| sunlight fix | toggle | `fadingFix`; row hidden when `BoardConfig::hasTouch()` (as upstream) |

## 4. `WrenSetupSystemActivity` (design `SetupSystem.html`)

Header `setup` / `system`. Rows:

| label | control | behaviour |
|---|---|---|
| sleep after | options 5 min, 10 min, 30 min | `sleepTimeoutMinutes`; value text is the stored value (`N min`); square chosen only on exact match; changing cycles 5→10→30 |
| wi-fi networks | value `N saved` + chevron | count from `WIFI_STORE`; select opens upstream `WifiSelectionActivity` in the mode that lists/manages networks without auto-joining (read its ctor) |
| language | options english, русский, українська | `I18N.setLanguage` + persist exactly like `LanguageSelectActivity.cpp:55-59`; value = the language's own name, lowercase; other languages show their name with no square chosen; redraw all labels after a change |
| titles from books | toggle | `libraryUseMetadata`; on change `library::markLibraryIndexDirty()` |
| clear reading cache | plain row | select opens upstream `ClearCacheActivity` as a child |
| rebuild library | plain row | select: `library::markLibraryIndexDirty()` then `goHome()` (home shows its scanning state) |

Below the rows, not selectable: Label `wren version` left, version right (Mono 16). Add
`src/wren/WrenVersion.h` with `constexpr const char* WREN_VERSION = "0.1.0";`.

## Layout in all four orientations

Landscape is 792x528: lists that do not fit between the header and the bottom tabs scroll by rows so the selected
row stays visible. Nothing may overlap the bottom tabs. Text cut with an ellipsis (`drawTextFit`), never wrapped.

## Strings

Add `STR_WREN_SETUP_*` keys to `lib/I18n/translations/english.yaml`, `russian.yaml`, `ukrainian.yaml` (lowercase,
like the existing `STR_WREN_*`), then regenerate `I18nKeys.h` with `scripts/gen_i18n.py`. English words exactly as
in the design. Russian/Ukrainian: short and natural (e.g. «экран сна» / «екран сну»).

## Host test

Put pure logic (row lists, option index mapping for sleep screen / battery / sleep after / language, scroll window)
in `src/wren/setup/SetupState.h` and add a host test in `test/wren_layout/` next to `HomeStateTest.cpp`, registered
in its `CMakeLists.txt`.

## Done

The files above exist and compile in your judgement; list every file changed and anything you could not do.
