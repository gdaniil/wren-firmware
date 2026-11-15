#pragma once

#include "WrenFontIds.generated.h"

class GfxRenderer;

namespace wren::ui {

// Built-in UI fonts of the Wren design (flash, 1-bit, Latin + Cyrillic). Sizes are em pixels.
//   WREN_GOLOS_18_FONT_ID  secondary lines
//   WREN_GOLOS_24_FONT_ID  rows, body UI text
//   WREN_GOLOS_18_SEMIBOLD_FONT_ID  title plate on a placeholder cover
//   WREN_GOLOS_32_FONT_ID  home tabs, primary action labels
//   WREN_GOLOS_40_FONT_ID  screen titles
//   WREN_GOLOS_56_HEADLINE_FONT_ID  state headlines: ASCII, ru/uk letters and ’ only
//   WREN_GOLOS_160_DIGITS_FONT_ID  go-to number: space % - . 0-9 only
//   WREN_MONO_16_FONT_ID   labels and numbers (IBM Plex Mono SemiBold), drawn uppercase
void registerFonts(GfxRenderer& renderer);

}  // namespace wren::ui
