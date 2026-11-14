# R7: Reader settings

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours`. C++ (gnu++2a), PlatformIO + Arduino, ESP32-C3 baseline
(~380 KB RAM). Plan: `docs/ours/READING_UI_PLAN.md`. Typography: `docs/ours/READER_TYPOGRAPHY_SPEC.md`,
`src/wren/typography/ReaderTypography.h`. Drawing: `src/wren/ui/WrenChrome.h` (`drawRow` with `Marker::Options`
draws the option squares; Label text only through `drawLabel`/`labelWidth`). Pattern for a Wren activity:
`src/wren/reader/WrenChaptersActivity.{h,cpp}`, `WrenReaderMenuActivity` (plain `Activity`, `requestUpdate()` in
`onEnter`, input in `loop()`, draw in `render()`).

## Decision (Daniil, 14 Nov)

Settings changed here are **global** (all books), stored in the existing settings (`SETTINGS.saveToFile()`). No
per-book store.

## Goal

New activity `src/wren/reader/WrenReaderSettingsActivity.{h,cpp}` replaces `TextSettingsActivity` at its call site
in `EpubReaderActivity::onReaderMenuConfirm` case `TEXT_SETTINGS` (`src/activities/reader/EpubReaderActivity.cpp`,
search `MenuAction::TEXT_SETTINGS`). The existing result handler (remember the position, drop the section, reopen
the menu) stays; add: if `SETTINGS.orientation` changed while the screen was open, apply it the way the menu
result does (`applyOrientation`). `TextSettingsActivity` stays compiled and unchanged (setup screens use it).

## Screen (X3 portrait 528x792 = device px; all four orientations)

1. `drawMenuStatusBar` (28 px).
2. `drawHeader(renderer, 28, bookTitle, reader)`: crumb = book title, title `STR_WREN_SETTINGS_TITLE` ("reader").
3. Four rows (`drawRow`, value in Label style, `Marker::Options`, black square = current):
   | Label | Values (order = squares) | Setting |
   |---|---|---|
   | font (`STR_WREN_SET_FONT`) | literata, golos, bitter | `SETTINGS.wrenFont[SETTINGS.wrenMode]` (`wren::Font`) |
   | text size (`STR_WREN_SET_SIZE`) | small, standard, large | `SETTINGS.wrenMode` (`wren::Mode`); each mode keeps its own font and step, so the font row follows the mode |
   | alignment (`STR_WREN_SET_ALIGN`) | left, justified | `SETTINGS.setParagraphAlignment(...)` with the existing LEFT / JUSTIFIED constants |
   | orientation (`STR_WREN_MENU_ORIENTATION`, exists) | portrait, landscape | `SETTINGS.orientation`, same toggle mapping as the menu (`wren::reader::toggleOrientation`, `ReaderMenuState.h`); rotate the renderer at once like the menu does |
   No other rows (line spacing, margins, hyphenation, etc. come from the mode).
4. Preview box below the rows: 24 px gap, 20 px side margin, 3 px ink border, padding 16/18/20. Label `preview`
   (`STR_WREN_SET_PREVIEW`), 12 px below a paragraph drawn in the reader font that the current settings resolve to
   (`SETTINGS.getReaderFontId()` after the change; if the Wren SD font for that mode is missing this is the upstream
   font, which is the truth of what the book will use). Text: a fixed sample per UI language (`STR_WREN_SET_SAMPLE`,
   two or three sentences), wrapped to the box width with `renderer.wrappedText`, as many lines as fit the box
   height (box ends 54 px above the bottom tabs), left-aligned or justified (spread the spaces of every line except
   the last) per the alignment setting. This is not the layout engine: no hyphenation; say so in a code comment.
5. `drawButtonTabs`: `back`, `select`, ArrowUp, ArrowDown. `drawEdgeTabs(renderer, true, true)`.

## Input

- Up/Down pick the row (selected row inverted). Side page buttons (PageBack/Left = previous value, PageForward/Right
  = next value) change the value of the selected row, wrapping; the marker and the value text move. Confirm = next
  value. Back: save (`SETTINGS.saveToFile()` once, only if something changed) and finish (the reader's handler
  reflows the book).
- On a change redraw the screen with a fast refresh; full refresh on enter. Orientation change: full redraw in the
  new orientation.

## i18n

english / russian / ukrainian (`lib/I18n/translations/*.yaml`, end of file), lower case:
`STR_WREN_SETTINGS_TITLE`: reader / чтение / читання; `STR_WREN_SET_FONT`: font / шрифт / шрифт;
`STR_WREN_SET_SIZE`: text size / размер текста / розмір тексту; `STR_WREN_SET_ALIGN`: alignment / выравнивание /
вирівнювання; `STR_WREN_SET_PREVIEW`: preview / пример / приклад; values `STR_WREN_SET_SMALL` small / мелкий /
дрібний, `STR_WREN_SET_STANDARD` standard / обычный / звичайний, `STR_WREN_SET_LARGE` large / крупный / великий,
`STR_WREN_SET_LEFT` left / по левому краю / ліворуч, `STR_WREN_SET_JUSTIFIED` justified / по ширине / за шириною;
font names are not translated ("literata", "golos", "bitter"); `STR_WREN_SET_SAMPLE`: english "It was the best of
times, it was the worst of times, it was the age of wisdom, it was the age of foolishness." russian "Все счастливые
семьи похожи друг на друга, каждая несчастливая семья несчастлива по-своему. Всё смешалось в доме Облонских."
ukrainian "Як парость виноградної лози, плекайте мову. Пильно й ненастанно політь бур’ян. Чистіша від сльози вона
хай буде." Regenerate `lib/I18n/I18nKeys.h` with `scripts/gen_i18n.py`.

## Rules

Pure logic (value cycling per row, justified spacing per line) in a header under `src/wren/reader/` with host tests
in `test/wren_layout/` (tests include headers as `"src/wren/reader/..."`, not `"wren/reader/..."`). The wrapped
preview lines are computed on enter and on change, not per frame. Do NOT run the `firmware-handoff` skill or
independent reviews.

## Constraints

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`, `lib/Epub/`. Keep the uncommitted earlier work in the tree.

## Done

If your sandbox lets you, run `pio run -e default`, `./bin/clang-format-fix -g`, `pio run -t unit-tests` (env:
`source .venv/bin/activate; export PATH="$HOME/.platformio/packages/tool-cmake/bin:$HOME/.platformio/packages/tool-ninja:$PATH"`).
If PlatformIO cannot lock `~/.platformio`, do not work around it and do not ask; say so and finish the code.
Report: files changed, commands with pass/fail, choices where the brief left room.
