#pragma once

#include "components/themes/lyra/LyraTheme.h"

class WrenTheme : public LyraTheme {
 public:
  Rect drawPopup(const GfxRenderer& renderer, const char* message) const override;
  void fillPopupProgress(const GfxRenderer& renderer, const Rect& layout, int progress) const override;
};
