# Reading UI: work plan

Source: design canvas "Wren design", page "Wren Reading"
(https://claude.ai/code/artifact/39f6d4e6-fb4b-4e0c-a389-daf4099ba750), with its blue spec notes.
Written 14 Nov 2026 on `ours` @ `e8363f2f`. Typography values stay in `READER_TYPOGRAPHY_SPEC.md`.

## Decisions

1. **Our UI layer is a module, `src/wren/ui/`.** Fonts, icons and chrome (status line, rows, header, button
   tabs, edge tabs) live there and draw through `GfxRenderer`. The library and setup screens will reuse it later.
2. **UI fonts are built in (flash), not on SD.** Menus must work without the SD fonts. Golos Text 24 and 40
   regular, IBM Plex Mono 16 semibold, Latin + Cyrillic only, 1-bit. Design px = X3 device px (528x792 mocks).
3. **Screens are new activities** (`src/wren/reader/`), swapped in at the few call sites in
   `EpubReaderActivity`. Upstream activities stay compiled and unchanged.
4. **Typography follows the spec, not the mock.** Default alignment is left (spec), the mock is justified.
   Text size values are the three modes (small, standard, large).
5. **Positions and bookmarks are anchors** (spine + offset), page numbers derived at draw time.
6. **Reader menu is exactly the 8 rows of the mock** (Daniil, 14 Nov). Footnotes, dictionary, clippings, sync,
   night mode, auto-turn, screenshot and QR leave the menu; their code stays.
7. **No chapter header over the text** (Daniil, 14 Nov). The chapter title is whatever the book itself prints.

## Tasks

| # | Task | Depends on | Who |
|---|---|---|---|
| R0 | UI fonts (Golos 24/40, Plex Mono 16 SB) + pixel icon set from the canvas, font ids, registration | none | Codex Terra medium |
| R1 | Chrome module: status line, menu status bar, header (title / back crumb), list row (label, value, chevron, icon, option squares, inverted selection), bottom button tabs, side edge tabs; four orientations | R0 | Codex Terra high |
| R2 | Reader: new status line (clock, progress line, battery), two-press button hints | R1 | Codex Terra high |
| R3 | Reader menu: 8 rows, orientation toggle in place, add-bookmark feedback, refresh-cache double-press confirm | R1 | Codex Terra high |
| R4 | Go to page: big number, bar with ticks, percent + chapter, side ±1 / ▲▼ ±10, partial refresh | R1 | Codex Terra medium |
| R5 | Chapters: numbered list, start pages (dash until paginated), current chapter highlighted | R1 | Codex Terra medium |
| R6 | Bookmarks: count, page · chapter · date, two-line excerpt, hold back to delete, empty state | R1 | Codex Terra high |
| R7 | Reader settings: per-book font / mode / alignment / orientation, side buttons change value, live preview | R1, R2 | Codex Terra high |

Order: R0, R1, then R2 to R7 in the order above (the canvas brief's order). R4 and R5 may run in parallel
(no shared files).

## Proposed in the canvas, not confirmed (build, flag in summary)

- Hint bar hides after ~3 s or on a page turn; the other left button fires at once; first press swallowed.
- Add bookmark: label reads "bookmark added" ~1.5 s, no popup.
- Refresh book: second select within 3 s confirms.
- Chapters: nested TOC flattened; book without TOC shows one row "whole book".
- Bookmarks: hold back ~1 s deletes, no confirmation.
- Go to page: holding a button repeats.
- Reader settings: per-book values stored with the book cache.

## Status

- R0 done (14 Nov), by hand: `scripts/wren/build_ui_fonts.py` (Golos 24/40, Plex Mono SB 16 as `wren_mono_16`,
  exact em px via the upstream converter), `scripts/wren/build_icons.py` (7 icons from the canvas),
  `src/wren/ui/WrenFonts.*` registered from `main.cpp`. Flash +105 KB (default 87.0%), RAM +72 B.
- R1 done (14 Nov), Codex Terra high: `src/wren/ui/WrenChrome.*`, `WrenLayout.h`, `test/wren_layout`. Edge tabs
  follow upstream (X3/X4 Pro edge buttons only; X4 stacked side buttons get none). Three envs build,
  clang-format clean, 961/961 host tests.
- R2 done (14 Nov), Codex Terra high (stopped twice, scope cut) + fixes by hand: status line at the top
  (`drawReaderStatusLine`), status band reserved at the top (Wren margins top 48 / bottom 24), two-press hints
  (`src/wren/reader/HintState.h`, host tests). By hand: hints drawn in the page pass (no second refresh), hidden
  after the second press, Select threshold = `confirmLongPressThreshold()`. Section v78, layout signature v5.
  Three envs, clang-format, 963/963 host tests, `pio check` clean. Hint tab reads "menu", not the mock's
  "settings": Select opens the menu. Dropped from the status line: book/chapter title, BLE and auto-turn notes,
  bookmark flag.
- R2b (chapter label "chapter N") dropped from scope (Daniil, 14 Nov). No chapter header over the text.
- R3 done (14 Nov), Codex Terra high + fixes by hand: `src/wren/reader/WrenReaderMenuActivity.*`,
  `drawBookHeader`, 8 rows, orientation toggle in place, "bookmark added" for 1.5 s (menu reopens), refresh with a
  second select within 3 s, cache refresh reopens the book at the same position. By hand: `requestUpdate()` in
  `onEnter` (the menu never painted), reverted unrequested auto-turn changes in the reader loop. Header shows
  chapter pages n/total (book-wide pages do not exist). Three envs, clang-format, 964/964, `pio check` clean.
- Percent, not pages (Daniil, 14 Nov): go-to and chapters work in percent of the book until book-wide pagination
  exists (a separate task later). Fonts added for go-to: Golos 18 (secondary line) and Golos 160 digits only
  (13 KB).
- R4 done (14 Nov), Codex Terra medium + fixes by hand: `src/wren/reader/WrenGoToActivity.*`, `GoToState.h`.
  Percent 0-100, side ±1, up/down ±10 with repeat, chapter of the target, full refresh on enter then fast.
  By hand: test include path, Label text through new `drawLabel`/`labelWidth` (uppercase). Flash 87.6%.
  Mono font has no ▲▼, so the ru/uk hint uses words. Whole-screen fast refresh on change (no region refresh).
- R5 done (14 Nov), Codex Terra medium + test include fix by hand: `src/wren/reader/WrenChaptersActivity.*`,
  `ChaptersState.h`, `drawChapterRow` (62 px). Flattened TOC, start percent per chapter, current chapter selected,
  whole-book row without TOC, TOC read per visible row. 968/968 host tests, default builds (87.6%).
- R6 done (14 Nov), Codex Terra high (stopped once, resumed): `src/wren/reader/WrenBookmarksActivity.*`,
  `BookmarksState.h`, header count via `drawHeader` right label. Meta "34% · ch. 5 · name", two-line excerpt in the
  reader font (Golos 18 fallback), wrapped for the visible window only; hold Back 1 s deletes, no confirmation;
  empty state "no bookmarks yet / menu → add bookmark". No date: `BookmarkEntry` stores none and the file format
  stays. Heap ~5-7 KB for 20 bookmarks (Codex estimate). 970/970 host tests, default builds (87.5%).
- Reader settings are global, not per book (Daniil, 14 Nov).
- R7 done (14 Nov), Codex Terra high: `src/wren/reader/WrenReaderSettingsActivity.*`, `ReaderSettingsState.h`.
  Rows font / text size (mode) / alignment / orientation with option squares, side buttons and select change the
  value, preview of a sample paragraph in the resolved reader font (wrapped, justified by hand, no hyphenation).
  Global settings, saved once on exit; orientation applied by the reader on return.
- Final check (14 Nov): default 87.6% flash / 20.3% RAM, x4c 85.0%, x4pro 85.2%; clang-format clean; 972/972 host
  tests; `pio check` no defects. `src/network/`, `freeink-sdk/`, `lib/hal/` untouched. Not run on a device.

## Device checklist (X3)

1. Reader: status line at the top (clock, progress line, battery) in all four orientations; text starts below it.
2. Hints: first Back/Select shows the bar and does nothing else; second press acts; bar hides after 3 s and on a
   page turn; long Back still goes home.
3. Menu: opens and paints; 8 rows; orientation toggles in place; add bookmark shows "bookmark added" ~1.5 s;
   refresh asks for a second select and reopens the book at the same place; go home.
4. Go to: 160 px number fits in portrait; side ±1, up/down ±10, holding repeats; select jumps; back returns.
5. Chapters: current chapter selected; percents; side buttons page; book without TOC.
6. Bookmarks: excerpt font; hold Back 1 s deletes and the release does not close the screen; empty state.
7. Reader settings: each row cycles; preview changes font and alignment; book reflows on return.
8. Heap above 50 KB free after repeated open/close of every screen and several text-size changes (SD fonts).
9. Old section caches rebuild once (section v78).
