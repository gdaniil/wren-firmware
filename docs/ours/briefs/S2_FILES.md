# S2: Setup › files and the delete dialog

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours`. Builds on S1 (`docs/ours/briefs/S1_SETUP.md`):
`src/wren/setup/`, `WrenChrome`, `WrenSetupActivity` exist. ESP32-C3 baseline (~380 KB RAM, >50 KB free, no leaks).

Design, 528x792 portrait, 1-bit: `docs/ours/design/setup/Files.html` (list) and `DeleteFile.html` (dialog over the
list). Fonts: Wren fonts from `src/wren/ui/WrenFonts.h`. Icons (already generated, 32 px grid, draw at 1x):
`icons::Folder`, `icons::BookFile`, `icons::ImageFile`, `icons::Trash` (2x in the dialog).

Reference code: upstream `src/activities/home/FileBrowserActivity.cpp` (directory listing with `Storage`, hidden
files rule `SETTINGS.showHiddenFiles`, how files are opened, `deleteSelected()`); `src/util/BookCacheUtils.h`
(`removeBookFile` removes the book, its cache and clippings); `WrenHomeActivity::deleteBook` (recents + index).

## Constraints (hard)

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`, generated files (except `I18nKeys.h` via `scripts/gen_i18n.py`). New code in `src/wren/setup/` (dialog
in `src/wren/ui/`). Do not run builds/tests; I run them. Justify heap use in a comment where not obvious.

## 1. `WrenFilesActivity` (design `Files.html`)

- `drawHeader(renderer, 28, "setup", "files")`, then a path line: Mono 16 (not uppercased), 20 px side padding,
  10 px vertical padding, 3 px ink rule below. Long paths cut from the left with an ellipsis.
- Rows 64 px: 20 px padding, 32 px icon, 16 px gap, name in Golos 24 (ellipsis), right value in Label style,
  2 px divider. Selected row inverted (icon in paper).
  - `..` row first when not at `/`: folder icon, value `up`.
  - Folders (sorted A–Z, before files): value = number of visible entries inside. Count lazily only for rows
    on screen and cache per visible window; never hold more than the visible window of counts.
  - Files (sorted A–Z): icon BookFile for book formats the reader opens (epub, txt, xtc, xtch, ...: reuse the
    upstream check), ImageFile for bmp, BookFile otherwise; value = size `0.3 mb` (one decimal, `kb` under 0.1 mb).
- Footer above the bottom tabs, centred Label: `hold back: delete` (only when a file row is selected).
- Input: ▲▼ move (wrap, hold repeats, scrolls by rows), select: folder → enter, `..` → up, book → open in reader
  (`activityManager.goToReader(path)`), bmp → upstream `BmpViewerActivity` as a child, other → nothing.
  Short back: up one folder; at `/` → finish (back to setup). Hold back 1 s on a file → delete dialog. Folders
  are not deletable here. Bottom tabs: back · select · ▲ · ▼, edge tabs on.
- Memory: names of the current folder only; follow how `FileBrowserActivity::loadFiles` sizes its vector once.
- Hook it in: `WrenSetupActivity` row `files` opens `WrenFilesActivity` at `/` (replace the S1 placeholder).

## 2. `WrenDeleteDialog` (design `DeleteFile.html`), reusable

- Draw over the current screen: 50% dither on the whole screen except the bottom tab strip (checkerboard, 2x2 px
  cells as in the design), then a paper card: 32 px side inset, top 210 (centre vertically in landscape), 3 px ink
  border, 24 px padding. Inside: Trash 2x, 16 px gap, headline Golos 40 `delete <title>?` (cut with ellipsis), 12 px
  gap, body Golos 18 wrapped: `Removes the file from the card, with its bookmarks and reading progress.`, 20 px gap,
  2 px rule, 12 px gap, file name (Mono 16, not uppercased, ellipsis) left and size right.
- Bottom tabs: tab 1 `keep`, tab 2 `delete`, tabs 3–4 empty. Back = keep, Confirm = delete. Ignore the release
  of the long press that opened it (see how home uses `swallowConfirm`).
- Delete: `removeBookFile(path)`; on failure for a non-book file fall back to `Storage.remove`; then
  `RECENT_BOOKS.removeByPath` + save, `library::markLibraryIndexDirty()` if `library::isLibraryBookFile(path)`.
  Reload the folder, keep the selection index clamped.
- Use the same dialog in `WrenHomeActivity::deleteBook` instead of the upstream `ConfirmationActivity` (design:
  the library uses the same dialog as files). Title = book title there; file name + size in the footer line.

## Strings

`STR_WREN_FILES_*` keys in english/russian/ukrainian yaml (lowercase, English exactly as the design), regenerate
`I18nKeys.h`.

## Layout

All four orientations: list scrolls, nothing overlaps the bottom tabs; dialog card fits in landscape 792x528
(shrink body to fewer lines before overlapping).

## Host test

Pure logic in `src/wren/setup/FilesState.h` (sorting dirs-first, size formatting, path truncation from the left,
scroll window, icon kind by extension) with a host test in `test/wren_layout/` registered in its `CMakeLists.txt`.

## Done

List every file changed and anything not done.
