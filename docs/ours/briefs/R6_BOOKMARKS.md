# R6: Bookmarks

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours`. C++ (gnu++2a), PlatformIO + Arduino, ESP32-C3 baseline
(~380 KB RAM). Plan: `docs/ours/READING_UI_PLAN.md`. Drawing: `src/wren/ui/WrenChrome.h` (every Label-style text
goes through `drawLabel` / `labelWidth`; never draw `WREN_MONO_16_FONT_ID` directly). Fonts:
`src/wren/ui/WrenFonts.h`. Pattern for a Wren activity: `src/wren/reader/WrenChaptersActivity.{h,cpp}` (R5) and
`WrenReaderMenuActivity` (plain `Activity`, `requestUpdate()` in `onEnter`, input in `loop()`, draw in `render()`).

## Decisions

- Percent, not pages (Daniil, 14 Nov): the meta line shows the book percent, not a page number.
- No date: `BookmarkEntry` (`src/activities/reader/BookmarkEntry.h`) stores none, and the bookmark file format stays
  as it is. The design's date part is left out.

## Goal

New activity `src/wren/reader/WrenBookmarksActivity.{h,cpp}` replaces `EpubReaderBookmarksActivity` at its call
site in `EpubReaderActivity::onReaderMenuConfirm` case `BOOKMARKS` (`src/activities/reader/EpubReaderActivity.cpp`,
search `MenuAction::BOOKMARKS`). Same contract: constructed with `(renderer, mappedInput, epub, epubPath)`; finishes
with the same `ProgressChangeResult` that `EpubReaderBookmarksActivity::openSelectedBookmark`
(`src/activities/reader/EpubReaderBookmarksActivity.cpp:72-94`) builds (copy that logic exactly: xpath, percentage,
visible-text offset, spine index, page hints); cancelled on Back. Load and save with `BookmarkFile::load/save` as
upstream does (`:36`, `:220-240`). Upstream class stays compiled and unchanged.

## Screen (X3 portrait 528x792 = device px; all four orientations)

1. `drawMenuStatusBar` (28 px).
2. `drawHeader(renderer, 28, bookTitle, bookmarks)`: crumb = book title, title `STR_WREN_MENU_BOOKMARKS` (exists),
   and the count in Label style right-aligned on the title baseline (6 px above the title's bottom). Add an optional
   right-hand label parameter to `drawHeader` (default none) rather than a second function.
3. Entries, top to bottom, each: padding 16 / 20, 2 px bottom rule. Meta line in Label style:
   `34% · ch. 5` (`STR_WREN_BOOKMARK_META` with `%d` percent and `%d` chapter number = TOC index + 1 of the
   bookmark's spine; no TOC entry -> only the percent). If the entry has a user `name`, show it after the chapter:
   `34% · ch. 5 · name`. 10 px below: the excerpt (`summary`), at most two lines with an ellipsis, in the reader
   font (`SETTINGS.getReaderFontId()`); if that font cannot draw (id 0 or not loaded) use `WREN_GOLOS_18_FONT_ID`.
   Wrap once per change, not per frame (cache the wrapped lines with the entry list; at most the visible entries).
   Selected entry inverted (ink fill, paper text).
4. Footer hint, Label style, centred, 54 px above the bottom: `STR_WREN_BOOKMARKS_HINT` ("hold back: delete").
5. `drawButtonTabs`: `back`, `select`, ArrowUp, ArrowDown.

Empty state (no bookmarks): header count `0`; centred in the body, Row style, `STR_WREN_BOOKMARKS_EMPTY`
("no bookmarks yet"), 12 px below in Label style `STR_WREN_BOOKMARKS_EMPTY_HINT` ("menu → add bookmark"; if the
mono font lacks `→`, use "menu: add bookmark"). Tabs: `back` only. Confirm does nothing.

## Input

- Up/Down move the highlight (no wrap); the list scrolls by whole entries keeping the selection visible.
- Confirm: open the bookmark (result as above). Short Back: cancelled.
- Hold Back about 1 s: delete the highlighted bookmark at once, no confirmation, save, stay on the screen (empty
  state when the last one goes). The release after the hold must not also close the screen. Check how long Back
  is routed (`MappedInputManager::wasLongPressed`, and whether `ActivityManager` or the activity base treats a long
  Back globally) and make the hold reach this activity.
- Redraw with a fast refresh on change.

## i18n

english / russian / ukrainian (`lib/I18n/translations/*.yaml`, end of file), lower case:
`STR_WREN_BOOKMARK_META`: "%d%% · ch. %d" / "%d%% · гл. %d" / "%d%% · розд. %d";
`STR_WREN_BOOKMARKS_HINT`: hold back: delete / удерживайте назад: удалить / утримуйте назад: видалити;
`STR_WREN_BOOKMARKS_EMPTY`: no bookmarks yet / закладок пока нет / закладок поки немає;
`STR_WREN_BOOKMARKS_EMPTY_HINT`: menu → add bookmark / меню → добавить закладку / меню → додати закладку.
Regenerate `lib/I18n/I18nKeys.h` with `scripts/gen_i18n.py`.

## Rules

Pure logic (meta line formatting, scroll window over variable-height entries) in a header under `src/wren/reader/`
with host tests in `test/wren_layout/` (tests include headers as `"src/wren/reader/..."`). Heap: the bookmark
vector and its cached wrapped lines are allocated on enter and on delete only; say in the report roughly how much
for 20 bookmarks. Do NOT run the `firmware-handoff` skill or independent reviews.

## Constraints

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`, `lib/Epub/`, the bookmark file format. Keep the uncommitted earlier work in the tree.

## Done

If your sandbox lets you, run `pio run -e default`, `./bin/clang-format-fix -g`, `pio run -t unit-tests` (env:
`source .venv/bin/activate; export PATH="$HOME/.platformio/packages/tool-cmake/bin:$HOME/.platformio/packages/tool-ninja:$PATH"`).
If PlatformIO cannot lock `~/.platformio`, do not work around it and do not ask; say so and finish the code.
Report: files changed, commands with pass/fail, choices where the brief left room.
