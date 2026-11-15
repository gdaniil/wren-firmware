# Library (home) UI: work plan

Source: design canvas "Wren design", page "Wren Library"
(https://claude.ai/code/artifact/39f6d4e6-fb4b-4e0c-a389-daf4099ba750), boards `Lib-*` and their notes.
Written 14 Nov 2026 on `ours` @ `21a4f446`. Builds on the reading UI module (`READING_UI_PLAN.md`).

## Decisions

1. **Home is the library.** A new `WrenHomeActivity` (`src/wren/home/`) replaces upstream `HomeActivity` in
   `ActivityManager::goHome` (one line). Upstream home, library list and file browser stay compiled.
2. **Three tabs, one built.** The header draws library · send · setup. The side buttons switch tabs; until our
   send and setup screens exist they open upstream File Transfer and Settings.
3. **Real covers everywhere** (Daniil, 14 Nov): 1-bit thumbnails from the book, generated once and cached on the
   card by upstream code. A book without a cover gets a pattern chosen from its title.
4. **Rows are 112 px** as drawn (62x88 cover), not the 76 px of the token list.
5. **Percent, not pages**, as in the reader: the continue card shows chapter and percent, no "3/612".
6. **Not built, the function does not exist in the firmware:** filter by reading/new/finished, mark as
   finished, mark as new, the "err" row label, our own search screen and keyboard (search uses the upstream
   keyboard), a separate "card unreadable" screen (boot cannot tell it from "no card"), indexing progress
   ("14 of 42": the index builder has no progress callback).
7. **Focus is the one inverted block** (the note's proposal): the continue card is ink when focused, a 3 px frame
   otherwise; a focused row is ink with paper text.

## Fonts and icons (L0, done by hand, 14 Nov)

- `scripts/wren/build_ui_fonts.py`: Golos 32 (tabs, primary action), Golos 56 headline set (ASCII, ru/uk letters,
  ’), Golos 18 semibold (cover title plate). Existing fonts regenerate byte-identical.
- `scripts/wren/build_icons.py`: Wren, WrenDone, WrenScanning, WrenGlasses, EmptyShelf, Trash, ChevronBig
  (32 px grid, drawn at 1x, 2x or 4x).

## Tasks

| # | Task | Who |
|---|---|---|
| L0 | Fonts and icons | by hand, done |
| L1 | `WrenHomeActivity`: header, continue card, book list, covers, states (empty, nothing started, scanning), book menu, search, tab switching | Codex Terra high |

## Status

- L0 done.
- L1 (14 Nov): Codex Terra high stopped twice before the activity; it delivered `HomeState.h` (list order via a
  sorted skip table, focus, scroll, cover pattern, tab font choice), its host tests and the strings. The activity
  was written by hand: `src/wren/home/WrenHomeActivity.{h,cpp}`, text helpers in `WrenChrome`
  (`textWidth`, `drawTextFit`, `drawTextWrapped`, `wrappedLineCount`), `goHome` swapped.
- Input on the X3: front buttons search (Back) · select (Confirm) · ▲ (Left) · ▼ (Right); side buttons Up/Down
  switch tabs (Down = send, Up = setup). Hold select 1 s = book menu (open, delete from card). In search mode the
  first hint reads "back" and clears the query.
- Heap: 9 `Book` slots (strings of the visible window and the continue book), search results 2 bytes per indexed
  book (fallible, only while a query is active), one `Epub`/`Xtc` at a time for progress, cover paths and
  thumbnail generation.

## Device checklist (X3)

1. Boot lands on the library: header with Wren and three tabs, continue card focused (ink), rows below.
2. ▲/▼ move focus card → rows, holding repeats, list scrolls by rows; the card turns to an outline when a row
   has focus.
3. Covers: first visit draws patterns, then real covers appear one by one; second visit shows them at once.
   A book without a cover keeps its pattern; the card pattern has the title plate.
4. Row states: progress bar + %, "done" with the wren on the full bar, "new".
5. Select opens the book; hold select 1 s opens the menu (open, delete from card → confirm → list rebuilt).
6. Search: keyboard, matches by title or author, count in the section header, back clears.
7. Side buttons: Down opens File Transfer, Up opens Settings; going home returns here.
8. Empty card: "no books yet" screen, select opens File Transfer. Fresh card with books: no card, "pick a book".
9. Four orientations: landscape hides the card while a row is focused; nothing overlaps.
10. Heap above 50 KB after repeated home ↔ reader and scrolling through a long library.
