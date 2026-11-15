# N3: Wren keyboard (password and network name entry)

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours`. The tree has a lot of uncommitted Wren work (setup,
library, send): it is ours, keep it. C++ (gnu++2a), PlatformIO + Arduino, ESP32-C3 baseline (~380 KB RAM).
Another Codex task writes `src/wren/send/WrenWifiActivity.*` at the same time: do not touch `src/wren/send/`.

Design (geometry and words), 528x792 portrait, 1-bit: `docs/ours/design/send/WifiPassword.html`. Ignore the HTML
fonts; use Wren fonts (`src/wren/ui/WrenFonts.h`): keys Golos 24, function keys and labels Mono 16 (`drawLabel`),
field text Golos 32. Chrome helpers: `src/wren/ui/WrenChrome.{h,cpp}` (`drawMenuStatusBar` via
`wren::setup::status`, `drawHeader`, `drawButtonTabs`, `drawEdgeTabs`, `Marker::Toggle` look for the switch).

Engine to mirror: `src/activities/util/KeyboardEntryActivity.{h,cpp}` (upstream, read all of it) and
`src/activities/util/KeyboardLayoutSet.h` (`keyboard_layouts::startingLayout()`, `next()`, enabled layouts). Keep its
behaviour: same constructor signature and the same result (`KeyboardResult{text}`, cancelled result on back),
`InputType` Text / Password / Url, `maxLength`, UTF-8 safe insert and backspace, long-press delete clears, cursor
movement, the SDK layout tables (`freeink::ui::KeyboardLayout` for letters, the symbol/number layer) and the language
key cycling the enabled layouts, touch routing on touch boards (X4 Pro: keep `InteractionBuffer` / `TouchHoldRouter`
working with the new key rects). Only the drawing and the look change; where the SDK `keyboard()` component draws
keys today, draw them yourself from the layout table and register the same hit rects.

## Constraints (hard)

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`, `src/activities/util/KeyboardEntryActivity.*` (upstream stays as is). Do not edit the i18n yaml files,
`I18nKeys.h` or `test/wren_layout/CMakeLists.txt`: the other task adds the strings below, I register the test. New
code: `src/wren/ui/WrenKeyboardActivity.{h,cpp}` and, for pure logic, `src/wren/ui/KeyboardState.h` with a host test
`test/wren_layout/KeyboardStateTest.cpp`. Do not run builds or
tests; I run them. No heap in render or per-key paths; justify any allocation.

## Public API (the Wi-Fi task codes against exactly this)

```cpp
// src/wren/ui/WrenKeyboardActivity.h
class WrenKeyboardActivity final : public Activity {
 public:
  WrenKeyboardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string crumb, std::string title,
                       std::string fieldLabel, std::string initialText = "", size_t maxLength = 0,
                       InputType inputType = InputType::Text);
};
```

Result: `KeyboardResult{text}` on ok, `isCancelled` on back. Include `activities/util/KeyboardEntryActivity.h` for
`InputType` (do not redefine it).

## Layout (`WifiPassword.html`)

- Status bar, then `drawHeader(renderer, 28, crumb, title)` (crumb e.g. `wi-fi`, title = network name).
- Field block at y 132+20: `drawLabel(fieldLabel)` (e.g. "password"), 10 px gap, field 60 px tall, 3 px ink border,
  16 px side padding, Golos 32 text with a 3x32 px caret at the cursor. Password type: dots unless shown. Text longer
  than the field scrolls so the caret stays visible.
- Password type only: 14 px below, the switch (48x26, as `Marker::Toggle`) and Golos 18 "show password". It is a
  focus stop above the first key row: ▲ from row 0 moves to it, select toggles, ▼ goes back to the keys. When focused,
  draw it inverted inside a 3 px ink frame (follow how a selected row inverts in `WrenChrome`).
- Keys: grid of 10 columns at x 20..508, 4 px gap, rows 44 px tall, 4 px apart, bottom of the grid 56 px above the
  screen bottom (above the bottom tabs). Each key: 2 px ink border, letter centered Golos 24; function keys Mono 16
  uppercase: del, shift, 123 / abc, the language key (shows the NEXT layout as a short code: en, ru, ua; hide it when
  only one layout is enabled), space (4 columns wide), ok (2 columns, always ink-filled). Row widths as in the design:
  row 3 = shift (2) + 7 letters + "." , bottom = 123 (2) + lang (1) + space (4) + "-" (1) + ok (2). Cyrillic layouts
  have 11-12 letters per row: shrink key width to fit the row in 488 px instead of the 10-column grid.
- Selected key (button navigation): ink fill, paper text. ok keeps ink fill; when selected draw it with a 3 px
  paper inset frame so focus is still visible.
- Bottom tabs: back · select · ▲ · ▼ (`drawButtonTabs`), `drawEdgeTabs(true, true)`: side buttons move left/right
  along a row, front ▲▼ move between rows, select types, back cancels; hold select = long press as upstream.
- Landscape 792x528: field and switch at the top as in portrait, keys fill the width (20 px sides), row height 40;
  nothing overlaps the bottom tabs. Check all four orientations as upstream does.

## Strings

Use these ids (the other task adds them to the yaml files): `STR_WREN_KB_SHOW_PASSWORD` (show password),
`STR_WREN_KB_DEL`, `STR_WREN_KB_SHIFT`, `STR_WREN_KB_SPACE`, `STR_WREN_KB_OK`, `STR_WREN_KB_ABC`. "123" and the
language codes are literals.

## Host test

Row width fitting (10-key and 12-key rows in 488 px with 4 px gaps), selection movement between rows of different
lengths (column kept proportionally), the switch focus stop (password only), caret scroll window, language-key label
(next layout, hidden with one layout).

## Done

Files exist and compile in your judgement. Report every file changed and anything not done.
