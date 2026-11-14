#include "WrenFonts.h"

#include <EpdFont.h>
#include <EpdFontFamily.h>
#include <GfxRenderer.h>
#include <builtinFonts/wren_golos_160_digits.h>
#include <builtinFonts/wren_golos_18.h>
#include <builtinFonts/wren_golos_24.h>
#include <builtinFonts/wren_golos_40.h>
#include <builtinFonts/wren_mono_16.h>

namespace wren::ui {
namespace {

// Static objects: the renderer keeps pointers to them for the program's lifetime, no heap.
EpdFont golos18Font(&wren_golos_18);
EpdFontFamily golos18Family(18, &golos18Font);
EpdFont golos24Font(&wren_golos_24);
EpdFontFamily golos24Family(24, &golos24Font);
EpdFont golos40Font(&wren_golos_40);
EpdFontFamily golos40Family(40, &golos40Font);
EpdFont golos160DigitsFont(&wren_golos_160_digits);
EpdFontFamily golos160DigitsFamily(160, &golos160DigitsFont);
EpdFont mono16Font(&wren_mono_16);
EpdFontFamily mono16Family(16, &mono16Font);

}  // namespace

void registerFonts(GfxRenderer& renderer) {
  renderer.insertFont(WREN_GOLOS_18_FONT_ID, golos18Family);
  renderer.insertFont(WREN_GOLOS_24_FONT_ID, golos24Family);
  renderer.insertFont(WREN_GOLOS_40_FONT_ID, golos40Family);
  renderer.insertFont(WREN_GOLOS_160_DIGITS_FONT_ID, golos160DigitsFamily);
  renderer.insertFont(WREN_MONO_16_FONT_ID, mono16Family);
}

}  // namespace wren::ui
