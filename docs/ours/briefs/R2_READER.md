# R2: Reader screen (status line, chapter label, two-press button hints)

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours`. C++ (gnu++2a), PlatformIO + Arduino, ESP32-C3 baseline
(~380 KB RAM, no PSRAM). Plan: `docs/ours/READING_UI_PLAN.md`. Drawing primitives: `src/wren/ui/WrenChrome.h`
(R1, already in the tree; extend it, keep its style). Fonts: `src/wren/ui/WrenFonts.h`.

Design reference (X3 portrait 528x792, design px = device px): status line 40 px at the top of the page, text
below; on a chapter's first page a mono label "chapter N" above the text; hint bar of four bottom tabs over the
page bottom after the first press of Back or Select.

## 1. Status line (replaces the upstream reader status bar for EPUB)

- `EpubReaderActivity::renderStatusBar()` (`src/activities/reader/EpubReaderActivity.cpp:2479-2520`) stops calling
  `GUI.drawStatusBar` and calls a new `wren::ui::drawReaderStatusLine(renderer, clock, bookFraction,
  batteryPercent)` added to `WrenChrome`.
- Layout, current orientation, top of the screen: band 40 px, side padding 32, gap 16. Left: clock in Label style
  (empty string = no clock, no gap). Middle: `drawProgressLine` filling the remaining width, vertically centred,
  fraction = position in the whole book (same value `bookProgress` uses today, /100). Right: battery glyph (same
  as in `drawMenuStatusBar`; factor it into one shared helper) then 12 px gap then percent in Label style.
- Clock: `HalClock::formatTime` (`lib/hal/HalClock.h:44`), 24 h; unavailable clock -> empty. It is drawn only when
  a page renders (no timer); keep it that way.
- No book title, no BLE/auto-turn notes, no bookmark flag in the status line (the design drops them).

## 2. Margins

The reserved status band moves from the bottom to the top.
- Every viewport computation in `EpubReaderActivity` that adds the status bar height to the bottom margin
  (`:1686-1696`, and the matching code at `:511-523` and `:630-636` that must agree with it) reserves 40 px at
  the top instead, in both the Wren path (`readerRenderSpec(0,0).linePitchPx != 0`) and the upstream path.
- Wren path: `wren::resolve` (`src/wren/typography/ReaderTypography.cpp:36-42`) returns top 24 / bottom 34
  (doubled in large mode). Make it top = 40 (status band) + 8 and bottom = 24, doubled in large mode as now.
  Update `test/reader_typography` expectations.
- Bump `LAYOUT_SIGNATURE_VERSION` (`lib/Epub/Epub/ReaderRenderSpec.h`) and `SECTION_FILE_VERSION`
  (`lib/Epub/Epub/Section.cpp:86`) if the viewport or layout output changes for the same settings.

## 3. Chapter label

- On the first page of a spine item that maps to a TOC entry (`epub->getTocIndexForSpineIndex`), draw
  "chapter N" in Label style (uppercase via WrenChrome), N = TOC index + 1, at the left text margin, top of the
  text area. Other pages: no label.
- The first page must leave room for it, so text does not overlap: add a field to `ReaderRenderSpec`
  (`firstPageTopInsetPx`, default 0 = upstream) that the layout engine subtracts from the first page's height
  only. Use 40 px (16 px label line + 24 px gap). Keep the engine change minimal and gated by the field, like the
  existing Wren fields; mix it into the signature; bump the versions above once.
- Words: new i18n key `STR_WREN_CHAPTER_N` with `%d`: english "chapter %d", russian "глава %d", ukrainian
  "розділ %d" (`lib/I18n/translations/*.yaml`, add at the end; other languages fall back to English). Regenerate
  with `scripts/gen_i18n.py` as `.agents/rules/generated-files-cache.md#translation-workflow` says.

## 4. Two-press button hints

Applies to short presses of the two left front buttons, Back and Select (Confirm), while reading an EPUB page.
- State: hidden (default). The page uses the full screen; no hint bar.
- Hidden + short release of Back or Confirm: no action fires. The hint bar appears: `wren::ui::drawButtonTabs`
  with labels `back` · `menu` · ArrowUp · ArrowDown, over a paper band 40 px tall across the bottom (portrait
  physical bottom, the same edge the tabs use). Redraw with a fast refresh.
- Visible + short release of Back: the normal Back action (today's `handleBackNavigation` / footnote return).
  Visible + short release of Confirm: open the reader menu (today's Confirm path, `:965-975`).
- The bar hides on a page turn, or 3 s after it appeared with no input (needs a check in `loop()`; redraw the page
  without the bar). After hiding, the next press is a first press again.
- Long presses keep today's behaviour (long Back = home/file browser, long Confirm = the configured shortcut).
  The first press is swallowed, never queued. Touch devices (`gpio.hasTouch()`): no hint state, behave as today.
- New i18n keys for the tab words: `STR_WREN_TAB_BACK` ("back", "назад", "назад"), `STR_WREN_TAB_MENU`
  ("menu", "меню", "меню").

## Rules

- Read `.agents/rules/ui-activities.md` ("Input frames and ownership", "Orientation-aware logic") and
  `.agents/rules/generated-files-cache.md` ("Cache file format versioning", "Translation workflow") first.
- Status line, label and hints must be correct in all four orientations.
- No heap allocation in the draw path. Keep edits to upstream files small; put logic in `src/wren/` where possible
  (for example a small `wren::reader::HintState` with host tests in `test/wren_layout/` or a new test folder).
- Do NOT run the `firmware-handoff` skill or independent reviews; the orchestrator reviews (project override in
  `/Users/daniilhurevych/Wren/xteink-workspace-claude.md`).

## Constraints

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`. Only generated file you may regenerate: `lib/I18n/I18nKeys.h` (via the script).

## Done

- `pio run -e default` builds; `./bin/clang-format-fix -g` leaves no diff; `pio run -t unit-tests` passes.
  Use the normal `.pio` and `~/.platformio` (no isolated copies). Environment:
  `source .venv/bin/activate; export PATH="$HOME/.platformio/packages/tool-cmake/bin:$HOME/.platformio/packages/tool-ninja:$PATH"`.
- Report: files changed with one line each, version bumps, choices where the brief left room.
