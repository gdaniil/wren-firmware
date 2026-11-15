# Y1: Wren sleep screen (from a book, and with no book)

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours` (uncommitted Wren work is in the tree; keep it).
ESP32-C3 baseline: ~380 KB RAM, keep >50 KB free, no leaks across sleep/wake.

## Constraints (hard)

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`, generated files (`WrenIcons.generated.h`, `I18nKeys.h`, fonts). Do not edit yaml translations: the
strings below already exist. Do not run builds or tests; I run them. Other agents are editing at the same time:
touch ONLY the files listed under "Files". Do not touch `test/wren_layout/CMakeLists.txt`, `WrenChrome.*`,
`UITheme.cpp`, `main.cpp`, `BootActivity.cpp`, `CrashActivity.cpp`.

## Files

- New: `src/wren/system/WrenSleepScreen.h`, `src/wren/system/WrenSleepScreen.cpp`.
- New: `src/wren/home/BookInfo.h`, `src/wren/home/BookInfo.cpp` (shared helpers, see 3).
- Edit: `src/activities/boot_sleep/SleepActivity.cpp` (minimal), `src/wren/home/WrenHomeActivity.cpp` (only to
  use the extracted helpers), `src/wren/setup/WrenSetupDisplayActivity.cpp` (option label).

## Design

`docs/ours/design/system/Sleep.html` and `SleepNoBook.html` (528x792 portrait, 1-bit, paper = white, ink =
black). Always drawn in portrait (SleepActivity already sets portrait for the final screen).

- WAKE UP label at top 14 px, right 46 px: `drawLabel` with `STR_WREN_SLEEP_WAKE_UP` (the label style
  uppercases), 8 px gap, then `icons::WakeArrow` (9x12) at scale 2. Label and arrow vertically centred.
- Mascot `icons::WrenSleeping` at scale 6 (192 px), centred horizontally and vertically in the space between
  the top of the screen and the top of the bottom block.
- From a book (`APP_STATE.lastSleepFromReader` and `APP_STATE.openEpubPath` not empty): book card, 36 px from
  the sides and from the bottom, 3 px ink frame, padding 14 px vertical / 16 px horizontal, 16 px gaps:
  - Cover thumb 44x64 with a 2 px ink frame. Use the cached thumbnail the library uses (same path and
    `BaseTheme::drawCoverThumbFill` as `WrenHomeActivity::drawCover`); do not generate one here. No thumbnail:
    fill ink and draw paper horizontal lines every 5 px (the design's placeholder).
  - Title: Golos 24, one line, ellipsis (`drawTextFit`). Under it, 6 px gap, author in Golos 18, ellipsis.
    No title: file name without extension. No author: `STR_WREN_LIB_UNKNOWN_AUTHOR`.
  - Right column: percent as a label (`62%`), top-aligned with the title. No page number.
- No book (sleep from home/library/setup, or nothing opened): battery line, 36 px from the sides and bottom,
  40 px tall, 3 px ink rule on top; `STR_WREN_SLEEP_BATTERY` label bottom-left, bottom-right: battery icon
  (22x11 outline 2 px, 1 px inner gap, fill proportional to percent, 2x5 nub on the right, 1 px gap) then 8 px
  then `64%` label. Use `powerManager.getBatteryPercentage()`. Draw the battery icon with a local static helper
  in your file (do not edit WrenChrome).
- No clock, no status bar, no button hints.

## Behaviour

1. `SleepActivity::renderDefaultSleepScreen()` (`SleepActivity.cpp:822`): replace its body with a call to
   `wren::system::drawSleepScreen(renderer)` followed by `renderer.displayBuffer(kSleepClean)`. Never invert
   (Wren draws paper-light for both DARK and LIGHT). Keep the function so every fallback (custom, cover
   without cover, transparent) still lands on it.
2. Skip the "going to sleep" popup on the Wren path: in `SleepActivity::onEnter` (`:731-737`) do not call
   `GUI.drawPopup` when `SETTINGS.sleepScreen` is `DARK` or `LIGHT`. Every other mode keeps it. Do not change
   the transparent/quick-resume paths.
3. Extract from `WrenHomeActivity.cpp` into `src/wren/home/BookInfo.{h,cpp}`, namespace `wren::home`: the
   progress reader (`loadBookProgress`, around `:93` and `:203`) and the thumbnail path lookup (`thumbPath`,
   `:261`) as free functions. Home calls them; behaviour must stay identical. Sleep uses them for the open
   book. Title/author/cover come from `RECENT_BOOKS` (entry whose `path == APP_STATE.openEpubPath`); if no
   entry, fall back to the file name.
4. Setup › display, row "sleep screen" (`WrenSetupDisplayActivity.cpp:96-106`, `SetupState.h:13`): the option
   that maps to `DARK` is labelled `STR_WREN_SLEEP_SCREEN_WREN` and comes first: `wren · cover`. Do not change
   stored values.

## Memory

Sleep runs right before deep sleep but heap still matters (no leak on wake). Open at most one `Epub`/`Xtc`
for progress and release it before drawing; the thumbnail `HalFile` is closed before `displayBuffer`.
Justify any allocation in a comment.

## Done

List every file changed, anything not done, and anything you are unsure of.
