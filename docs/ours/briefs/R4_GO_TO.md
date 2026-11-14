# R4: Go to (percent of the book)

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours`. C++ (gnu++2a), PlatformIO + Arduino, ESP32-C3 baseline.
Plan: `docs/ours/READING_UI_PLAN.md`. Drawing: `src/wren/ui/WrenChrome.h`. Fonts: `src/wren/ui/WrenFonts.h`
(`WREN_GOLOS_160_DIGITS_FONT_ID` holds only space % - . 0-9; `WREN_GOLOS_18_FONT_ID` for secondary lines). Look at
`src/wren/reader/WrenReaderMenuActivity.{h,cpp}` (R3) as the pattern for a Wren activity: plain `Activity`,
`requestUpdate()` in `onEnter`, input in `loop()`, draw everything in `render()`.

## Decision (Daniil, 14 Nov)

Book-wide page numbers do not exist in this firmware (chapters are paginated only when opened). This screen works
in **percent of the book**, not pages. Page mode comes later as a separate task.

## Goal

New activity `src/wren/reader/WrenGoToActivity.{h,cpp}` replaces `EpubReaderPercentSelectionActivity` at its one
call site, `EpubReaderActivity::onReaderMenuConfirm` case `GO_TO_PERCENT`
(`src/activities/reader/EpubReaderActivity.cpp:1288-1300`). Same contract: constructed with the initial percent,
returns `PercentResult` (`src/activities/ActivityResult.h:31`) on confirm, cancelled on Back. The upstream class
stays compiled and unchanged.

## Screen (X3 portrait 528x792 = device px; all four orientations)

1. `drawMenuStatusBar` (28 px), clock + battery as in R3.
2. `drawHeader(renderer, 28, bookTitle, "go to")`: crumb = book title, title `STR_WREN_GOTO_TITLE`. Pass the book
   title in the constructor.
3. Body between the header rule and the bottom tabs, side padding 20, the block vertically centred:
   - Row: `ChevronLeft` (scale 4) at the left edge, the big number centred, `ChevronRight` (scale 4) at the right
     edge. Big number = target percent with a `%` sign, `WREN_GOLOS_160_DIGITS_FONT_ID`, tracking -10.
     If "100%" does not fit the width between the chevrons (landscape is fine, portrait 528 may be tight),
     drop the `%` sign.
   - 36 px below: bar 20 px tall, 2 px ink border, filled to target/100.
   - 6 px below: ticks 3x12 at 0, 25, 50, 75 % of the bar width.
   - 8 px below, Label style: `0` left, `50` centre, `100` right.
   - 32 px below: 3 px rule, then 16 px padding, then one line in `WREN_GOLOS_18_FONT_ID`: the title of the TOC
     chapter that contains the target percent (map percent to a spine index with `epub->getCumulativeSpineItemSize`
     / `getBookSize`, then `getTocIndexForSpineIndex` / `getTocItem(...).title`; no TOC entry -> empty line).
     Ellipsis to the width. Pass the `std::shared_ptr<Epub>` in the constructor.
   - Hint line in Label style, centred, 54 px above the bottom edge: `STR_WREN_GOTO_HINT` ("side ±1 · up/down ±10").
4. `drawButtonTabs`: `back` (`STR_WREN_TAB_BACK`), `select` (`STR_WREN_TAB_SELECT`), ArrowUp, ArrowDown.
5. `drawEdgeTabs(renderer, true, true)`.

## Input

- Side page buttons (PageBack / PageForward, and Left / Right): -1 / +1. Front Up / Down: +10 / -10 (Up = more).
  Clamp to 0..100. Holding a button repeats (use the existing `ButtonNavigator` pattern from
  `EpubReaderPercentSelectionActivity` if it fits; otherwise ~4 steps per second after a 500 ms hold).
- Confirm: finish with `PercentResult{target}`. Back: cancelled (the reader reopens the menu, as today).
- On change redraw only the number, the bar and the chapter line, with a fast (partial) refresh; a full redraw
  only on enter. If the renderer has no region refresh, redraw the whole screen with a fast refresh.

## i18n

Add to english / russian / ukrainian (`lib/I18n/translations/*.yaml`, end of file):
`STR_WREN_GOTO_TITLE`: go to / перейти / перейти; `STR_WREN_GOTO_HINT`: side ±1 · up/down ±10 / сбоку ±1 · ▲▼ ±10 /
збоку ±1 · ▲▼ ±10. Check that `▲▼±·` exist in `wren_mono_16` (`lib/EpdFont/builtinFonts/wren_mono_16.h`
intervals); if a glyph is missing use plain words ("up/down", "+-"). Regenerate `lib/I18n/I18nKeys.h` with
`scripts/gen_i18n.py`.

## Rules

Pure logic (clamp and step, percent -> spine index) in a header under `src/wren/reader/` with host tests in
`test/wren_layout/`. No heap allocation per frame. Do NOT run the `firmware-handoff` skill or independent reviews.

## Constraints

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`, `lib/Epub/`. Keep the uncommitted R0-R3 work in the tree. Build-time regeneration of `I18nStrings.*`
and `src/network/html/*.generated.h` by `pio run` is fine.

## Done

If your sandbox lets you, run `pio run -e default`, `./bin/clang-format-fix -g`, `pio run -t unit-tests`
(env: `source .venv/bin/activate; export PATH="$HOME/.platformio/packages/tool-cmake/bin:$HOME/.platformio/packages/tool-ninja:$PATH"`).
If PlatformIO cannot lock `~/.platformio`, do not work around it and do not ask; say so in the report and finish
the code. Report: files changed, commands with pass/fail, choices where the brief left room.
