# R3: Reader menu (Wren design)

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours`. C++ (gnu++2a), PlatformIO + Arduino, ESP32-C3 baseline
(~380 KB RAM, no PSRAM). Plan: `docs/ours/READING_UI_PLAN.md`. Drawing: `src/wren/ui/WrenChrome.h` (status bar,
header, rows, tabs, progress bar; extend it in its style if needed). Fonts: `src/wren/ui/WrenFonts.h`.

## Goal

A new activity `src/wren/reader/WrenReaderMenuActivity.{h,cpp}` replaces `EpubReaderMenuActivity` when the
reader opens its list menu (`EpubReaderActivity::openReaderMenu`, `src/activities/reader/EpubReaderActivity.cpp:344-382`).
It returns the same `MenuResult` (`src/activities/ActivityResult.h:20`) with
`EpubReaderMenuActivity::MenuAction` values, so `onReaderMenuConfirm` (`:1210-1372`) keeps working. The upstream
menu class stays compiled and unchanged. The toolbar menu style (`usesToolbarMenu`) is left as it is.

## Screen (X3 portrait 528x792 = device px; current orientation, all four must work)

Top to bottom:
1. `drawMenuStatusBar` (28 px): clock (HalClock::formatTime 24 h, empty if unavailable) and battery.
2. Header block 104 px, side padding 20, 3 px ink rule at its bottom: book title (Title style, ellipsis) at the
   top of the block; 12 px below, `drawProgressBar` full width (fraction = book progress); 8 px below, a line in
   Label style: left "n/total" (the chapter page numbers available today: `position.displayPage()` /
   `position.totalPages`; omit when total is 0), right "N%" (book percent). Title + bar + line bottom-aligned
   14 px above the rule. Add this as `wren::ui::drawBookHeader(...)` in WrenChrome.
3. Rows (`drawRow`, 54 px), exactly these, in this order (no other rows; Daniil's decision):
   | Label (i18n key) | Marker | MenuAction |
   |---|---|---|
   | reader settings (`STR_WREN_MENU_READER_SETTINGS`) | chevron | TEXT_SETTINGS |
   | chapters (`STR_WREN_MENU_CHAPTERS`) | chevron | SELECT_CHAPTER |
   | add bookmark (`STR_WREN_MENU_ADD_BOOKMARK`) | icon `Bookmark` scale 2 | TOGGLE_BOOKMARK |
   | bookmarks (`STR_WREN_MENU_BOOKMARKS`) | value = count, chevron | BOOKMARKS |
   | go to... (`STR_WREN_MENU_GO_TO`) | chevron | GO_TO_PERCENT |
   | orientation (`STR_WREN_MENU_ORIENTATION`) | value portrait/landscape (`STR_WREN_PORTRAIT`, `STR_WREN_LANDSCAPE`), 2 option squares | ROTATE_SCREEN |
   | refresh book and delete cache (`STR_WREN_MENU_REFRESH`) | icon `Refresh` scale 1 | DELETE_CACHE |
   | go home (`STR_WREN_MENU_GO_HOME`) | icon `Home` scale 2 | GO_HOME |
   If the rows do not fit (landscape), the list scrolls by whole rows keeping the selection visible.
4. Bottom: `drawButtonTabs` with `close` (`STR_WREN_TAB_CLOSE`), `select` (`STR_WREN_TAB_SELECT`), ArrowUp,
   ArrowDown.

Selection: the selected row is drawn inverted. Up/Down (front ▲▼, and side page buttons) move it; Confirm acts;
Back closes (cancelled result, like `closeCancelled`). Initial selection: the last used action, like upstream
(`selectedMenuAction`).

## Row behaviour

- Chevron rows: finish with the action (existing reader handling opens the screen).
- orientation: toggles in place on Confirm between portrait and landscape. Black square = current. Mapping:
  current portrait or inverted -> landscape CW; current landscape CW or CCW -> portrait. Rotate at once the way
  upstream does (`EpubReaderMenuActivity.cpp:138-152`: `ReaderUtils::applyOrientation` on the renderer only, the
  value goes back in `MenuResult.orientation`), redraw the menu in the new orientation.
- add bookmark: saves the current position and the menu stays. Do it by finishing with TOGGLE_BOOKMARK; in
  `onReaderMenuConfirm` the TOGGLE_BOOKMARK case adds the bookmark and reopens the menu with a flag so that row
  reads "bookmark added" (`STR_WREN_MENU_BOOKMARK_ADDED`) for about 1.5 s, then returns to "add bookmark"
  (redraw only on that change). The bookmarks count updates. No popup.
- refresh book and delete cache: first Confirm changes the row label to "select again to refresh"
  (`STR_WREN_MENU_REFRESH_CONFIRM`); a second Confirm within 3 s finishes with DELETE_CACHE; moving the
  selection or 3 s passing resets it. In `onReaderMenuConfirm`, DELETE_CACHE must reopen the book at the same
  position instead of going home (today it calls `onGoHome()` at `:1338`): after clearing the cache, rebuild and
  render the saved position.
- go home: finish with GO_HOME.

## i18n

Add every `STR_WREN_*` key above to `lib/I18n/translations/english.yaml`, `russian.yaml`, `ukrainian.yaml` (end
of file; others fall back to English), lower case like the design:
english / russian / ukrainian —
reader settings / настройки чтения / налаштування читання; chapters / главы / розділи; add bookmark / добавить
закладку / додати закладку; bookmark added / закладка добавлена / закладку додано; bookmarks / закладки /
закладки; go to... / перейти… / перейти…; orientation / ориентация / орієнтація; portrait / книжная / книжкова;
landscape / альбомная / альбомна; refresh book and delete cache / обновить книгу и кэш / оновити книгу й кеш;
select again to refresh / ещё раз, чтобы обновить / ще раз, щоб оновити; go home / на главную / на головну;
close / закрыть / закрити; select / выбрать / вибрати.
Regenerate `lib/I18n/I18nKeys.h` with `scripts/gen_i18n.py`. Build steps that regenerate `I18nStrings.*` or
`src/network/html/*.generated.h` as part of `pio run` are fine.

## Rules

- Read `.agents/rules/ui-activities.md` ("Input frames and ownership", "Activity lifecycle and memory
  management", "Orientation-aware logic") first. Look at how `EpubReaderMenuActivity` handles input and
  rendering, and mirror the lifecycle; you may base the activity on the plain `Activity` class and draw with
  WrenChrome directly (no FreeInkUI list needed).
- No heap allocation per frame. Pure logic (orientation toggle mapping, confirm timer, scroll window) in a small
  header under `src/wren/reader/` with host tests in `test/wren_layout/`.
- Do NOT run the `firmware-handoff` skill or independent reviews. Do not ask for permission for normal build
  outputs; just run the commands.

## Constraints

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`, `lib/Epub/`.

## Done

`pio run -e default` builds; `./bin/clang-format-fix -g` leaves no diff; `pio run -t unit-tests` passes. Normal
`.pio`. Env: `source .venv/bin/activate; export PATH="$HOME/.platformio/packages/tool-cmake/bin:$HOME/.platformio/packages/tool-ninja:$PATH"`.
Report: files changed, commands with pass/fail, choices where the brief left room. If you stop early, say why.
