#pragma once

#include <cstdint>

namespace wren {

enum class Mode : uint8_t { Small, Standard, Large };
enum class Font : uint8_t { Literata, GolosText, Bitter };

struct Hyphenation {
  float zone;
  uint8_t minWordLetters, minLeft, minRight, maxConsecutive;
};
struct ModeSpec {
  float xHeightMm[5];
  uint8_t defaultStep;  // Zero-based; the specification uses one-based steps.
  Font defaultFont;
  float lineStepXh, sideMarginMm, indentEm, spaceLines, stemRatio, statusXHeightMm;
  Hyphenation hyphenation;
};
struct ResolvedTypography {
  int xHeightPx, linePitchPx, sideMarginPx, topMarginPx, bottomMarginPx, statusXHeightPx;
  float indentEm, spaceLines;
  Hyphenation hyphenation;
  uint16_t regularWeight, boldWeight;
};

enum class StepResult : uint8_t { Moved, OfferNeighbour, AtBoundary };
constexpr int X3_PPI = 259;
constexpr int X4_PPI = 217;
int mmToPx(float mm, int ppi);
const ModeSpec& modeSpec(Mode mode);
const char* fontFamily(Font font);
ResolvedTypography resolve(int ppi, Mode mode, uint8_t step, Font font, const char* language);
StepResult nextStep(Mode mode, uint8_t& step);
StepResult previousStep(Mode mode, uint8_t& step);
}  // namespace wren
