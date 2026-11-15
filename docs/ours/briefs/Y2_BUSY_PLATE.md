# Y2: Busy plate (Wren popup)

Repo: `/Users/daniilhurevych/Wren/firmware`, branch `ours` (uncommitted Wren work is in the tree; keep it).
ESP32-C3 baseline: ~380 KB RAM, no PSRAM.

## Constraints (hard)

No flashing, no `erase_flash`/`write_flash`, no commits, no push. Do not edit `src/network/`, `freeink-sdk/`,
`lib/hal/`, generated files (`WrenIcons.generated.h`, `I18nKeys.h`, fonts). Do not edit yaml translations: the
strings below already exist. Do not run builds or tests; I run them. Other agents are editing at the same time:
touch ONLY the files listed under "Files". Do not touch `test/wren_layout/CMakeLists.txt`, `WrenChrome.*`,
`SleepActivity.cpp`, `main.cpp`, `BootActivity.cpp`, `CrashActivity.cpp`, `src/wren/system/`.

## Files

- New: `src/wren/ui/WrenBusyPlate.h`, `src/wren/ui/WrenBusyPlate.cpp`.
- New: `src/wren/ui/WrenTheme.h`, `src/wren/ui/WrenTheme.cpp`.
- Edit: `src/components/UITheme.cpp` (`setTheme`, `:39-73`): the `LYRA` and `COVER_GRID` case builds
  `WrenTheme` instead of `LyraTheme`, metrics stay `LyraMetrics::values`. One include, one type. Nothing else in
  that file. No new enum value (`uiTheme` is a key in `/api/settings`).

## Design

`docs/ours/design/system/BusyPlate.html` (528x792 portrait, 1-bit). Read the "Busy plate" note copied below.

- Card 448 px wide in portrait (32 px from the sides). Top at 252 px in portrait; in landscape centre it
  vertically and horizontally, width `min(448, screenWidth - 64)`.
- 8 px paper margin around card and shadow (fill paper first, so page text never touches it), then a solid ink
  shadow rect offset 8 px right and down, then the card: paper fill, 3 px ink frame, padding 20 top/bottom,
  16 left, 24 right.
- Left: mascot at scale 3 (96 px). Right, 20 px gap: headline Golos 40 (line 44 px), one line, ellipsis; under
  it an optional label line (`drawLabel`, mono 16 uppercase). Text block vertically centred against the sprite.
  If a headline does not fit one line at Golos 40, draw it in Golos 24 wrapped to 2 lines instead.
- Static: no animation, no spinner. Ink and paper only, no grey.

## API

```cpp
namespace wren::ui {
enum class BusyPose { Wait, Sleep, Error };  // Wait = icons::WrenHello, Sleep = icons::WrenSleeping, Error = icons::WrenConfused
// Draws the plate over the current frame and returns the card rect. Does not refresh the display.
Rect drawBusyPlate(const GfxRenderer& renderer, BusyPose pose, const char* headline, const char* label = nullptr);
void drawBusyProgress(const GfxRenderer& renderer, const Rect& card, int percent);  // no refresh either
}
```

Progress bar (for `fillPopupProgress`): inside the card, in the text column under the headline, 12 px gap,
14 px tall, 2 px ink frame, fill = percent. Reserve no space when there is no progress (callers draw the plate
first, then progress).

## WrenTheme

- `drawPopup(renderer, message)`: map known messages by comparing with `tr(...)` (strcmp):
  - `STR_ENTERING_SLEEP` → Sleep pose, `STR_WREN_BUSY_SLEEP`
  - `STR_LOADING_POPUP` → Wait, `STR_WREN_BUSY_WAIT`
  - `STR_INDEXING` → Wait, `STR_WREN_BUSY_INDEXING`
  - `STR_SAVE_PROGRESS_FAILED` → Error, `STR_WREN_BUSY_SAVE_FAILED`, label `STR_WREN_BUSY_CARD_BUSY`
  - `STR_FONT_DAMAGED` → Error, `STR_WREN_BUSY_FONT_DAMAGED`, label `STR_WREN_BUSY_DEFAULT_FONT`
  - anything else → Wait pose, the message as given.
  Then `renderer.displayBuffer()` exactly as `BaseTheme::drawPopup` does (`BaseTheme.cpp:772-801`), and return
  the card rect.
- `fillPopupProgress(renderer, layout, progress)`: `drawBusyProgress` then
  `renderer.displayBuffer(HalDisplay::FAST_REFRESH)`, like `BaseTheme.cpp:803-826`.
- Check every caller of `drawPopup`/`fillPopupProgress` (`grep -rn "drawPopup\|fillPopupProgress" src`). Some
  rely on the returned rect or on the popup height (e.g. `drawSleepPopupPreservingFrame` in
  `SleepActivity.cpp:648` saves a band from `popupTopOffsetRatio` + `UI_12` line height). List any caller whose
  assumptions the bigger plate breaks; do not edit those files, report them.

## Fonts and icons

Wren fonts: `src/wren/ui/WrenFonts.h` (font ids). Text helpers: `src/wren/ui/WrenChrome.h` (`drawTextFit`,
`drawTextWrapped`, `drawLabel`, `drawIcon`). Icons: `src/wren/ui/WrenIcons.generated.h`.

## Done

List every file changed, the caller report, anything not done, anything you are unsure of.

## The design note (verbatim)

One component for every wait popup. Override drawPopup in the Wren theme; all callers get it. Card: 448 px
wide (32 px from the screen sides), top at 252 px. 3 px ink frame, paper fill, solid ink shadow offset 8 px right
and down (a filled rect, no grey). 8 px paper margin around card and shadow so page text never touches it. Left:
Wren ×3 (96 px). Pose by context: sleeping Wren for going to sleep, the Boot Wren for waits, the confused Wren
for errors. Right: headline ui-heading 40 px, one line; under it one ui-label line of context. Static: no
animation, no spinner. Fast refresh on show; the screen underneath redraws when the work ends.
