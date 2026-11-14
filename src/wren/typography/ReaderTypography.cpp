#include "ReaderTypography.h"

#include <cmath>
#include <cstring>

#include "WrenFontMetrics.generated.h"

namespace wren {
namespace {
constexpr ModeSpec MODES[] = {
    {{1.1f, 1.2f, 1.3f, 1.4f, 1.5f}, 1, Font::Literata, 2.5f, 1.2f, 1, 0, .21f, 1, {.75f, 8, 2, 2, 2}},
    {{1.4f, 1.5f, 1.7f, 1.9f, 2.15f}, 0, Font::Literata, 2.7f, 1.6f, 1, 0, .20f, 1, {.75f, 8, 2, 2, 2}},
    {{2.4f, 2.7f, 3.1f, 3.5f, 4.0f}, 0, Font::GolosText, 2.9f, 1.2f, 0, .5f, .23f, 2, {.60f, 8, 2, 2, 2}},
};
}  // namespace
int mmToPx(float mm, int ppi) { return static_cast<int>(std::round(mm * ppi / 25.4f)); }
const ModeSpec& modeSpec(Mode mode) { return MODES[static_cast<unsigned>(mode) < 3 ? static_cast<unsigned>(mode) : 1]; }
const char* fontFamily(Font font) {
  constexpr const char* FAMILIES[] = {"WrenLiterata", "WrenGolos", "WrenBitter"};
  const auto index = static_cast<unsigned>(font);
  return FAMILIES[index < 3 ? index : 0];
}
ResolvedTypography resolve(int ppi, Mode mode, uint8_t step, Font font, const char* language) {
  const unsigned m = static_cast<unsigned>(mode) < 3 ? static_cast<unsigned>(mode) : 1;
  const unsigned f = static_cast<unsigned>(font) < 3 ? static_cast<unsigned>(font) : 0;
  if (step >= 5) step = MODES[m].defaultStep;
  const auto& s = MODES[m];
  const int xh = mmToPx(s.xHeightMm[step], ppi);
  auto hyphenation = s.hyphenation;
  if (language && std::strncmp(language, "en", 2) == 0 && (language[2] == '\0' || language[2] == '-'))
    hyphenation.minRight = 3;
  const FontMetrics* metrics = nullptr;
  for (const auto& entry : WREN_FONT_METRICS[f]) {
    if (entry.xHeightPx == xh) metrics = &entry;
  }
  // Large mode doubles the fixed vertical margins with its doubled status size.
  const int scale = m == 2 ? 2 : 1;
  return {xh,
          static_cast<int>(std::round(s.lineStepXh * xh)),
          mmToPx(s.sideMarginMm, ppi),
          24 * scale,
          34 * scale,
          mmToPx(s.statusXHeightMm, ppi),
          s.indentEm,
          s.spaceLines,
          hyphenation,
          static_cast<uint16_t>(metrics ? metrics->regularWeight : 0),
          static_cast<uint16_t>(metrics ? metrics->boldWeight : 0)};
}
StepResult nextStep(Mode mode, uint8_t& step) {
  if (step < 4) {
    ++step;
    return StepResult::Moved;
  }
  return mode == Mode::Large ? StepResult::AtBoundary : StepResult::OfferNeighbour;
}
StepResult previousStep(Mode mode, uint8_t& step) {
  if (step > 0 && step < 5) {
    --step;
    return StepResult::Moved;
  }
  return mode == Mode::Small ? StepResult::AtBoundary : StepResult::OfferNeighbour;
}
}  // namespace wren
