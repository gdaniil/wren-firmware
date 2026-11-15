#pragma once

#include "components/themes/BaseTheme.h"

class GfxRenderer;

namespace wren::ui {
enum class BusyPose { Wait, Sleep, Error };
Rect drawBusyPlate(const GfxRenderer& renderer, BusyPose pose, const char* headline, const char* label = nullptr);
// Everything drawBusyPlate paints: card, shadow and the paper margin.
Rect busyPlateBounds(const GfxRenderer& renderer);
void drawBusyProgress(const GfxRenderer& renderer, const Rect& card, int percent);
}  // namespace wren::ui
