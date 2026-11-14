# R1: Wren chrome module

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours`. C++ (gnu++2a), PlatformIO + Arduino, ESP32-C3 baseline
(~380 KB RAM, no PSRAM). Plan: `docs/ours/READING_UI_PLAN.md`.

## Goal

A small drawing module, `src/wren/ui/WrenChrome.{h,cpp}`, that the next tasks (reader, reader menu, chapters,
bookmarks, go-to, reader settings) use to draw the Wren design. It only draws with `GfxRenderer`
(`lib/GfxRenderer/GfxRenderer.h`). It does not own input, activities or state. No heap allocation (no
`std::string`, `std::vector`, `new`); use stack buffers when text must be uppercased.

Already in the tree (R0, do not regenerate):
- Fonts, registered at boot: `src/wren/ui/WrenFonts.h` -> `WREN_GOLOS_24_FONT_ID` (rows), `WREN_GOLOS_40_FONT_ID`
  (titles), `WREN_MONO_16_FONT_ID` (labels, numbers). Each family has a regular style only.
- Icons: `src/wren/ui/WrenIcons.generated.h`, `wren::ui::Icon {width, height, rows}`, one `uint32_t` per row,
  bit `(width - 1 - x)` = ink. Icons: `ChevronRight`, `ChevronLeft` (3x6), `ArrowUp`, `ArrowDown` (6x3),
  `Bookmark`, `Home` (12x12), `Refresh` (32x32).

## Visual spec (design px = device px, X3 portrait 528x792)

Colors: ink = black, paper = white (1-bit, no gray). "Inverted" means paper on ink.

Text styles (tracking = `drawText(..., tracking)` in whole px, `GfxRenderer.h:352`):
- `Title`: Golos 40, tracking -2. `Row`: Golos 24, tracking -1. `Label`: Mono 16, tracking +1, always UPPERCASE
  (ASCII and Cyrillic: lower->upper for U+0430-044F, U+0450-045F, U+0491; others unchanged).

Elements (all must work in the current renderer orientation; use `renderer.getScreenWidth/Height()`):
1. `drawIcon(renderer, icon, x, y, scale, ink)`: each set bit is a `scale`x`scale` filled rect. `ink=false` draws
   paper (for inverted rows).
2. `drawMenuStatusBar(renderer, clock, batteryPercent)`: band 28 px tall at the top, side padding 20, content
   6 px from the top. Left: clock in Label style. Right: battery glyph then percent in Label style, gap 8.
   Battery glyph: outline 22x11, 2 px stroke, inner fill proportional to percent with 1 px padding; nub 2x5 at
   the right, 1 px gap. Callers pass the clock string (may be empty: then nothing on the left).
3. `drawHeader(renderer, y, crumb, title)`: band 104 px tall starting at `y`, side padding 20, bottom rule 3 px
   ink. If `crumb` is non-empty: at top+12, `ChevronLeft` scale 2 then 8 px gap then crumb in Label style.
   Title in Title style, baseline 12 px above the rule, truncated with ellipsis to the width.
   Returns the y below the rule.
4. `Row` (struct) + `drawRow(renderer, y, row)`: 54 px tall, side padding 20, gap 16 between parts, bottom rule
   2 px ink. Parts left to right: label (Row style, single line, ellipsis), optional value (Label style),
   then one trailing marker: none / chevron (`ChevronRight` scale 2, centred in a 24 px column) / icon (any
   Icon, scale given, centred in a 24 px column) / option squares (N squares 14x14, 2 px border, gap 6, the
   chosen one filled; N up to 5). `selected=true` fills the row ink and draws everything in paper (the rule
   stays). Returns the next y.
5. `drawButtonTabs(renderer, labels[4])`: the four front-button tabs, always in physical portrait like
   `BaseTheme::drawButtonHints` (`src/components/themes/BaseTheme.cpp:217-251`: switch to Portrait, draw,
   restore). Tabs: x = {38,154,268,384} when portrait width >= 528 else {25,130,245,350}; width 106, height 30,
   bottom-aligned to the screen edge, 3 px ink border on top/left/right (no bottom border), paper fill, content
   centred. A label is either text (Label style) or one of the special values for `ArrowUp`/`ArrowDown`
   (scale 2). Empty label = no tab. Skip all drawing when `gpio.hasTouch()` (same as upstream).
6. `drawEdgeTabs(renderer, showLeft, showRight)`: side page-button tabs like `BaseTheme::drawSideButtonHints`
   (`BaseTheme.cpp:253-...`, same y=155, height 80, only when `gpio.hasEdgeSideButtons()`). Width 14,
   3 px ink border except on the screen edge side, paper fill; left tab shows `ChevronLeft`, right tab
   `ChevronRight` (scale 2), centred. Follow exactly how upstream maps them in each orientation.
7. `drawProgressLine(renderer, x, y, w, fraction)`: 2 px track at y+2, 6x6 marker at the position (left edge at
   `x + fraction*(w-6)`), total height 6. `drawProgressBar(renderer, x, y, w, fraction)`: 8 px box with 2 px
   border, filled to fraction (min 6 px when fraction > 0).

Layout helpers that hold arithmetic (tab x positions per width, row count that fits between two y values,
truncation width) go in a pure header `src/wren/ui/WrenLayout.h` with no Arduino/GfxRenderer includes, and get a
host test `test/wren_layout/` registered in `test/CMakeLists.txt` like `test/reader_typography/`.

## Also

- A tiny demo is NOT wanted; no activity changes in this task.
- Read `.agents/rules/ui-activities.md` sections "Orientation-aware logic" and `.agents/rules/coding-standards.md`
  before writing. Comments short.

## Constraints

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/` or generated files (`*.generated.h`, `lib/EpdFont/builtinFonts/*`, `lib/I18n/I18nKeys.h`). Do not
edit upstream files except `test/CMakeLists.txt` (one line).

## Done

- `pio run -e default` builds. `./bin/clang-format-fix -g` leaves no diff. `pio run -t unit-tests` passes.
  (Environment: `source .venv/bin/activate; export PATH="$HOME/.platformio/packages/tool-cmake/bin:$HOME/.platformio/packages/tool-ninja:$PATH"`.)
- Report: files added, public API, any place the spec was ambiguous and what you chose.
