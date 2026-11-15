# L1: Wren home = library screen

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours`. C++ (gnu++2a), PlatformIO, Arduino, ESP32-C3 baseline
(~380 KB RAM, no PSRAM). All sizes below are device pixels on the X3 in portrait (528 x 792); the design is 1:1.

## Hard constraints

- No flashing, no `erase_flash` / `write_flash`, no commits, no push.
- Do not edit `src/network/`, `freeink-sdk/`, `lib/hal/` or generated files (`lib/EpdFont/builtinFonts/*`,
  `*.generated.h`, `lib/I18n/I18nKeys.h`). `src/wren/ui/WrenIcons.generated.h` and the font headers are already
  generated for you: use them, do not regenerate.
- Our code goes in new files under `src/wren/home/`. Upstream files get only the minimal edits listed below.
- Every heap allocation needs a reason in a short comment; prefer fixed buffers. No allocation proportional to
  the library size except what is listed here.
- Follow the patterns of the existing Wren activities (`src/wren/reader/WrenChaptersActivity.*`,
  `WrenReaderMenuActivity.*`) and the chrome module `src/wren/ui/WrenChrome.{h,cpp}` (extend it where a
  primitive is shared; keep screen-specific drawing in the activity).
- Do not run builds, formatting or tests and do not report on them; the reviewer runs them. Do make sure the code
  compiles in your head: correct includes, signatures that exist.

## What exists (use it)

- Fonts (`src/wren/ui/WrenFonts.h`, ids in `WrenFontIds.generated.h`): `WREN_GOLOS_18_FONT_ID`,
  `WREN_GOLOS_18_SEMIBOLD_FONT_ID`, `WREN_GOLOS_24_FONT_ID`, `WREN_GOLOS_32_FONT_ID`, `WREN_GOLOS_40_FONT_ID`,
  `WREN_GOLOS_56_HEADLINE_FONT_ID` (ASCII + ru/uk letters + ’ only), `WREN_MONO_16_FONT_ID` (labels, drawn
  uppercase via `wren::ui::drawLabel` / `labelWidth`).
- Icons (`WrenIcons.generated.h`, `wren::ui::drawIcon(renderer, icon, x, y, scale, ink)`): `Wren`, `WrenDone`,
  `WrenScanning`, `WrenGlasses`, `EmptyShelf`, `Trash`, `ChevronBig` (all 32x32), `ArrowUp`, `ArrowDown`,
  `ChevronLeft`, `ChevronRight`.
- Chrome: `drawMenuStatusBar` (28 px status bar: clock left, battery right), `drawButtonTabs` (bottom hints at
  x = 38, 154, 268, 384, 106 x 30), `drawEdgeTabs` (side hints at y 155..235), `drawProgressBar`, `drawLabel`.
  Check how the reader activities get the clock string and battery percent and do the same.
- Library index: `lib/LibraryIndex/LibraryIndexFile.h` (+ `LibraryBuilder.h`). Open/rebuild exactly like
  `LibraryListActivity::onEnter` / `rebuildIndex` (`src/activities/library/LibraryListActivity.cpp:83-141`):
  `library::isLibraryIndexDirty()`, `index.open(library::libraryIndexPath())`,
  `header().metadataEnabled != (SETTINGS.libraryUseMetadata != 0)`, `library::buildLibraryIndex("/", stats, ...)`.
  Rows by `index.ordinalForRow(library::SortOrder::RecentDesc, row)` then `readRecord`, `readTitle` (fallback
  `readName`), `readAuthor`, `readPath`.
- Recently opened books: `RECENT_BOOKS.getBooks()` (`src/RecentBooksStore.h`), `pruneMissing()`.
  `LibraryListActivity::resolvePinned` (`:162-200`) shows how recents are matched to index rows.
- Progress: `loadBookProgress(path)` (`src/util/BookProgress.h`), -1 = never opened, 100 = finished. It loads an
  `Epub`, so call it only for visible rows and cache the result per row in the visible window.
- Covers: `Epub::getThumbBmpPath(height)`, `Epub::generateThumbBmpFromSource(height)`, XTC equivalents
  (`Xtc::getThumbBmpPath(height)`, `load()` + `generateThumbBmp(height)`). See `HomeActivity::loadGridCover`
  (`src/activities/home/HomeActivity.cpp:123-165`) and the BMP draw at `src/components/themes/BaseTheme.cpp:505-535`
  (`HalFile`, `Bitmap bitmap(file); bitmap.parseHeaders()`, `renderer.drawBitmap(bitmap, x, y, maxW, maxH)`).
- Navigation: `activityManager.goToReader(path)`, `goToFileTransfer()`, `goToSettings()`. Delete with
  confirmation: copy `LibraryListActivity::promptDeleteBookByPath` (`:401-436`: `ConfirmationActivity`,
  `removeBookFile`, `RECENT_BOOKS.removeByPath`, rebuild index). Search keyboard: `LibraryListActivity::openSearch`
  (`:438-466`, `KeyboardEntryActivity`) and the filter in `applyFilter` (`:626-672`), including its case folding.

## Upstream edits allowed (minimal, one each)

1. `src/activities/ActivityManager.cpp` `goHome`: construct `WrenHomeActivity` instead of `HomeActivity`
   (keep the include of HomeActivity only if still needed elsewhere). The `initialMenuItem` /
   `cleanInitialRefresh` arguments may be ignored except a clean (full) refresh on first paint after boot.
2. `lib/I18n/translations/{english,russian,ukrainian}.yaml`: the strings below, at the end of the Wren block.

## The screen

Ink #151515 = black, paper = white. 1-bit only, no gray. Radius 0 everywhere. Side padding 20.

From top (portrait):

1. **Status bar**, 28 px: `drawMenuStatusBar`.
2. **Header**, 80 px tall, padding 0 20, items centred vertically, gap 16: the `Wren` icon at scale 2 (64x64), then
   the tab box filling the rest: 56 px tall, 3 px ink frame, three equal cells split by 3 px ink rules. Labels
   Golos 32, centred. The library cell is the current tab: ink fill, paper text. Others: paper, ink text. If a
   label is wider than its cell minus 8 px, draw that label in Golos 24.
3. **Continue card** (only when there is a continue book, see Data), margin top 12, x 20..508, 232 tall, padding 16,
   gap 20. Focused: ink fill, paper content. Not focused: paper fill, 3 px ink frame, ink content.
   - Cover box 132 x 200 at the left (see Covers), 2 px frame in the content colour.
   - Right column: label `continue · ch. N` (mono label; `ch. N` = TOC index of the saved spine + 1 via
     `epub->getTocIndexForSpineIndex`; omit ` · ch. N` when unknown or not EPUB). 14 px below: title, Golos 40,
     at most 2 lines, word-wrapped, ellipsis on the second line. 10 px below: author, Golos 18, one line.
   - Bottom of the right column: progress bar 10 px tall (2 px frame, fill = percent, minimum 6 px fill when
     percent > 0), 10 px below it the percent as a label (`41%`). No page count.
4. **Section header**, margin top 12, 40 px, padding 0 20, 3 px ink rule at the bottom: label `all books` left,
   book count right (count of every book in the index). In search mode: label = the query, right = match count.
5. **Book rows**, 112 px each, padding 0 20, gap 16, 2 px ink rule at the bottom of each row:
   - Cover box 62 x 88, 2 px ink frame.
   - Middle column (flex): title Golos 24, one line, ellipsis; 2 px below, author Golos 18, one line, ellipsis.
     No author: `unknown author`. No title: the file name without extension.
   - Right column 112 wide:
     - progress 1..99: bar 8 px tall (2 px frame, fill), 8 px below the percent label.
     - progress 100: a solid ink bar 8 px tall; `WrenDone` icon (1x, 32x32) standing on the bar, right edge 2 px
       from the bar end, its bottom 6 px above the bar; label `done` 8 px below the bar.
     - never opened (-1): label `new`, right aligned, vertically centred.
   - Focused row: ink fill over the whole row (x 0..width), paper text, labels, bars and icons inverted.
   - As many rows as fit above the bottom hints (bottom 40 px reserved). Scroll by whole rows; keep the focused
     row visible.
6. **Bottom hints** (`drawButtonTabs`): `search`, `select`, ▲, ▼. When a book (card or row) is focused, add a
   small plate above the select hint: ink fill, paper mono label `menu`, 24 px tall, 3 px frame open at the
   bottom, sitting on top of the select hint, centred on it (width = label + 16).
7. **Edge tabs**: `drawEdgeTabs(renderer, true, true)` (side buttons switch tabs).

### Covers

- Every book with a cover image shows its real cover: thumbnail at the box's inner height (196 for the card,
  84 for rows), drawn centred and cropped/limited to the inner box. Generate a missing thumbnail on first
  display (one book at a time, Epub/Xtc object on the heap and released before the next one, as in
  `loadGridCover`). While generating, draw the screen first with the pattern placeholder, then generate, then
  repaint (fast refresh). Remember per path that generation failed this session so it is not retried on every
  paint (a small fixed array of path hashes is enough).
- No cover (or generation failed): a pattern filling the inner box, chosen from a hash of the title, one of:
  (a) solid ink, (b) 1 px checker (2 px tile), (c) 45° hatch, 1 px line every 4 px, (d) dots: 1 px dot on a 3 px
  grid, (e) horizontal lines, 1 px every 5 px. On the continue card the pattern also gets the title plate:
  paper box at the bottom of the cover (inset 8), title in Golos 18 semibold, ink, padding 4 5, up to 3 lines.
- Covers in a focused (inverted) row or card stay as they are; only the 2 px frame takes the content colour.

### Data

- Continue book = `RECENT_BOOKS.getBooks()[0]` if its file exists and it is in the index; not finished books
  only is NOT required (show it whatever the progress).
- List order: the other recent books (`getBooks()[1..]`) in their order, then every other indexed book by
  `RecentDesc`, skipping the continue book and the recents already shown. The continue book does not appear in
  the list. Reuse the pinned-recents approach of `LibraryListActivity` (`resolvePinned`, fixed arrays sized
  `RecentBooksStore::MAX_RECENT_BOOKS`).
- Only the visible window is materialized (titles, authors, progress, cover paths): fixed arrays sized to the
  maximum visible rows (8 is enough for every orientation), never one string per book.

### Input

- Up / Down: move focus card → rows; wrap is off (stop at the ends). Holding repeats like the reader lists.
- Select short press: open the focused book (`activityManager.goToReader`).
- Select long press (`mappedInput` long-press threshold used by the reader, `confirmLongPressThreshold()` or
  the library's 1000 ms): **book menu** (below).
- Left bottom button (Back position, hint `search`): open the search keyboard. A non-empty query switches the
  list to search mode (matches in title or author, case-folded as in `applyFilter`); the continue card is hidden
  in search mode and focus starts on the first match. Back in search mode clears the query. The match list may
  use one fallible `uint16_t` per indexed book, as upstream does.
- Side buttons (page buttons, the ones `drawEdgeTabs` labels): next/previous tab. Tab send →
  `activityManager.goToFileTransfer()`, tab setup → `activityManager.goToSettings()`. Library stays.
- Back with nothing to clear: nothing.

### Book menu (overlay)

Over the whole screen except the bottom 40 px: a 2 px checker pattern (ink every other pixel, the screen below
stays visible through it). Panel: x 32..width-32, top 170, paper, 3 px ink frame. Inside: title Golos 24 +
author Golos 18 in a block with padding 16 24 and a 3 px rule under it; then rows 64 px tall, padding 0 24,
Golos 24, 2 px rules between them: `open`, `delete from card` (the `Trash` icon at the right of that row). The
focused row is ink with paper text. Hints: `close`, `select`, ▲, ▼. Back = close. Delete → the same confirmation
and delete as `promptDeleteBookByPath`, then back to the list with focus clamped.

### States

- **Empty library** (index has 0 books): under the header, a block vertically centred in the remaining space
  (padding 0 20, 120 px bottom): `EmptyShelf` at 4x (128 px); 24 below the headline `no books yet`, Golos 56;
  14 below the body Golos 18, wrapped to the width: `Send books from your phone over Wi-Fi, or copy them to the
  microSD card.`; 28 below a 72 px button, ink fill, padding 0 20: `send books` Golos 32 paper on the left,
  `ChevronBig` 1x paper on the right. Select → `goToFileTransfer()`. Hints: only `select`, ▲, ▼ (empty first slot).
- **Nothing started** (books exist, recents empty): no continue card; focus starts on the first row. Below the
  last row, if 120+ px are left above the hints: margin 36 20, `WrenGlasses` at 2x, 16 px below `pick a book`
  Golos 40. Hidden once any book has been opened (recents non-empty).
- **Scanning** (the index must be rebuilt on enter): before the blocking rebuild, paint the header and, in
  place of the continue card, a box (margin 12 20, 3 px ink frame, padding 20): `WrenScanning` 2x, 20 px gap,
  label `scanning microsd` and below it Golos 40 `looking for books`; under the box a label `keep the card in`.
  Then rebuild and paint the normal screen.

### Orientation

Use `renderer.getScreenWidth()` / `getScreenHeight()` for everything; nothing at a hard-coded 528 or 792 except
where the design is absolute (tab hint x positions come from `drawButtonTabs`). Landscape (792 x 528): same
layout, the card stays 232 px, fewer rows fit. All four orientations must lay out without overlap.

### Refresh

First paint after entering: full refresh when `cleanInitialRefresh`, else the usual. Focus moves: fast refresh.
Cover generation repaint: fast refresh.

## Strings (en / ru / uk)

| Key | en | ru | uk |
|---|---|---|---|
| STR_WREN_TAB_LIBRARY | library | книги | книги |
| STR_WREN_TAB_SEND | send | приём | прийом |
| STR_WREN_TAB_SETUP | setup | опции | опції |
| STR_WREN_LIB_CONTINUE | continue | читать | читати |
| STR_WREN_LIB_CHAPTER | ch. %d | гл. %d | розд. %d |
| STR_WREN_LIB_ALL_BOOKS | all books | все книги | усі книги |
| STR_WREN_LIB_NEW | new | новая | нова |
| STR_WREN_LIB_DONE | done | прочитана | прочитана |
| STR_WREN_LIB_UNKNOWN_AUTHOR | unknown author | автор неизвестен | автор невідомий |
| STR_WREN_TAB_SEARCH | search | поиск | пошук |
| STR_WREN_LIB_OPEN | open | открыть | відкрити |
| STR_WREN_LIB_DELETE | delete from card | удалить с карты | видалити з картки |
| STR_WREN_LIB_EMPTY_TITLE | no books yet | книг пока нет | книжок поки немає |
| STR_WREN_LIB_EMPTY_BODY | Send books from your phone over Wi-Fi, or copy them to the microSD card. | Отправьте книги с телефона по Wi-Fi или скопируйте их на карту microSD. | Надішліть книжки з телефона через Wi-Fi або скопіюйте їх на картку microSD. |
| STR_WREN_LIB_SEND_BOOKS | send books | отправить книги | надіслати книжки |
| STR_WREN_LIB_PICK | pick a book | выберите книгу | оберіть книжку |
| STR_WREN_LIB_SCANNING | scanning microsd | читаю карту | читаю картку |
| STR_WREN_LIB_LOOKING | looking for books | ищу книги | шукаю книжки |
| STR_WREN_LIB_KEEP_CARD | keep the card in | не вынимайте карту | не виймайте картку |

Reuse existing `STR_WREN_TAB_SELECT`, `STR_WREN_TAB_CLOSE`, `STR_WREN_TAB_MENU` for the hints and the plate.

## Tests

Put the pure logic (list order with the continue book and pinned recents, focus movement and scroll start,
which state to show, pattern choice from a title hash, tab label fallback decision) in a header-only state
file `src/wren/home/HomeState.h` with host tests in `test/wren_layout/HomeStateTest.cpp` (add it to
`test/wren_layout/CMakeLists.txt`, same style as `ChaptersStateTest.cpp`).

## Done =

New files `src/wren/home/WrenHomeActivity.{h,cpp}`, `HomeState.h`, the test, the `goHome` edit, the strings.
Report: files touched, every heap allocation and its bound, anything from this brief you could not do and why.
