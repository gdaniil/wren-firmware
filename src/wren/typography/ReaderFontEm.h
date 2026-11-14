#pragma once

#include "ReaderTypography.h"
#include "WrenFontMetrics.generated.h"

namespace wren {
inline uint16_t fontEmHundredths(Font font, int xHeightPx) {
  const auto index = static_cast<unsigned>(font);
  if (index >= 3) return 0;
  for (const auto& entry : WREN_FONT_METRICS[index]) {
    if (entry.xHeightPx == xHeightPx) return (entry.emPx64 * 100U + 32) / 64;
  }
  return 0;
}
}  // namespace wren
