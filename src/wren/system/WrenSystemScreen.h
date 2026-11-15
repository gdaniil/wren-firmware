#pragma once

#include "wren/ui/WrenIcons.generated.h"

class GfxRenderer;

namespace wren::system {
void drawStateScreen(GfxRenderer& renderer, const wren::ui::Icon& sprite, const char* headline, const char* body,
                     const char* tab, int bottomPadding, const char* reason = nullptr);
void drawNoCardScreen(GfxRenderer& renderer);
}  // namespace wren::system
