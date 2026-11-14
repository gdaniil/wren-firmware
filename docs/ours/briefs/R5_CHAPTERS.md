# R5: Chapters

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours`. C++ (gnu++2a), PlatformIO + Arduino, ESP32-C3 baseline
(~380 KB RAM). Plan: `docs/ours/READING_UI_PLAN.md`. Drawing: `src/wren/ui/WrenChrome.h`. Fonts:
`src/wren/ui/WrenFonts.h`. Every Label-style text (numbers, percents, hints) goes through
`wren::ui::drawLabel` / `labelWidth` (uppercase, tracking); never draw `WREN_MONO_16_FONT_ID` directly. Pattern for a Wren activity: `src/wren/reader/WrenReaderMenuActivity.{h,cpp}` (plain
`Activity`, `requestUpdate()` in `onEnter`, input in `loop()`, draw in `render()`), and `WrenGoToActivity` (R4).

## Decision (Daniil, 14 Nov)

No book-wide page numbers yet. The right column shows the **percent of the book where the chapter starts**, not a
page number.

## Goal

New activity `src/wren/reader/WrenChaptersActivity.{h,cpp}` replaces `EpubReaderChapterSelectionActivity` at its
call site in `EpubReaderActivity::onReaderMenuConfirm` case `SELECT_CHAPTER`
(`src/activities/reader/EpubReaderActivity.cpp:1221-1257`). Same contract: constructed with the epub and the
current TOC index, returns `ChapterResult{spineIndex, anchor}` (`src/activities/ActivityResult.h:26`) on confirm,
cancelled on Back. Upstream class stays compiled and unchanged. Read how it pages through TOC entries without
loading them all (`EpubReaderChapterSelectionActivity.cpp:47-98`: `getTocItem` is an SD read) and keep that: only
the visible rows are read per render; never hold the whole TOC in RAM.

## Screen (X3 portrait 528x792 = device px; all four orientations)

1. `drawMenuStatusBar` (28 px).
2. `drawHeader(renderer, 28, bookTitle, chapters)`: crumb = book title, title `STR_WREN_MENU_CHAPTERS` (exists).
3. Rows 62 px tall (not the 54 px `drawRow`; add a `drawChapterRow` to WrenChrome, same look): side padding 20,
   gap 16, 2 px bottom rule. Left: two-digit number (`01`, `02`, ... `99`, then `100`...) in Label style in a 28 px
   column (wider when needed). Middle: title, Row style, one line, ellipsis. Right: start percent in Label style
   (e.g. `34%`). Selected row inverted.
   - Number = TOC index + 1. Nested TOC is flattened: every entry is a row, no indent.
   - Start percent = `round(100 * getCumulativeSpineItemSize(spineIndex - 1) / getBookSize())` (0 for the first
     spine item). Entries whose spine index is -1: dash `—` (check the glyph exists in `wren_mono_16`; else `-`).
   - Book without TOC entries: one row "whole book" (`STR_WREN_CHAPTERS_WHOLE_BOOK`) that jumps to spine 0.
4. `drawButtonTabs`: `back`, `select`, ArrowUp, ArrowDown (existing `STR_WREN_TAB_*` keys).
5. `drawEdgeTabs(renderer, true, true)`.

## Input

- Opens with the current chapter selected and scrolled into view.
- Up/Down move the selection by one (wrap like the R3 menu). Side page buttons move by one screen page.
- The list scrolls by whole rows; keep the selection visible (reuse `wren::reader::scrollStart` from
  `src/wren/reader/ReaderMenuState.h`).
- Confirm: finish with `ChapterResult`. Back: cancelled.
- Redraw with a fast refresh on selection change.

## i18n

`STR_WREN_CHAPTERS_WHOLE_BOOK`: whole book / вся книга / уся книга (english / russian / ukrainian yaml, end of
file). Regenerate `lib/I18n/I18nKeys.h` with `scripts/gen_i18n.py`.

## Rules

Pure logic (percent from sizes, number formatting, page jumps) in a header under `src/wren/reader/` with host
tests in `test/wren_layout/`. No heap allocation per frame beyond the `TocEntry` strings `getTocItem` returns for
visible rows. Do NOT run the `firmware-handoff` skill or independent reviews.

## Constraints

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`, `lib/Epub/`. Keep the uncommitted earlier work in the tree.

## Done

If your sandbox lets you, run `pio run -e default`, `./bin/clang-format-fix -g`, `pio run -t unit-tests` (env:
`source .venv/bin/activate; export PATH="$HOME/.platformio/packages/tool-cmake/bin:$HOME/.platformio/packages/tool-ninja:$PATH"`).
If PlatformIO cannot lock `~/.platformio`, do not work around it and do not ask; say so and finish the code.
Report: files changed, commands with pass/fail, choices where the brief left room.
