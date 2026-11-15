#include "WrenSystemScreen.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <algorithm>

#include "wren/ui/WrenChrome.h"
#include "wren/ui/WrenFonts.h"

namespace wren::system {
void drawStateScreen(GfxRenderer& renderer, const ui::Icon& sprite, const char* headline, const char* body,
                     const char* tab, int bottomPadding, const char* reason) {
  using namespace ui;
  constexpr int top = 28;
  const int width = renderer.getScreenWidth() - 40;
  const int bottom = renderer.getScreenHeight() - 40;
  const bool hasReason = reason && *reason;
  const int titleLines =
      std::max(1, wrappedLineCount(renderer, WREN_GOLOS_56_HEADLINE_FONT_ID, headline, width, 2, -2));
  int bodyLines = std::max(1, wrappedLineCount(renderer, WREN_GOLOS_18_FONT_ID, body, width, 10));
  int reasonLines = hasReason ? std::max(1, wrappedLineCount(renderer, WREN_MONO_16_FONT_ID, reason, width, 4)) : 0;
  const int fixed = 128 + 24 + titleLines * 53 + 14 + (hasReason ? 28 + 3 + 12 + 19 + 8 : 0);
  const int available = bottom - top;
  while (fixed + bodyLines * 25 + reasonLines * 22 > available && bodyLines > 1) --bodyLines;
  while (fixed + bodyLines * 25 + reasonLines * 22 > available && reasonLines > 1) --reasonLines;
  const int height = fixed + bodyLines * 25 + reasonLines * 22;
  int y = top + std::max(0, (available - height - bottomPadding) / 2);

  renderer.clearScreen();
  char clock[16] = {};
  halClock.formatTime(clock, sizeof(clock), false);
  drawMenuStatusBar(renderer, clock, powerManager.getBatteryPercentage());
  drawIcon(renderer, sprite, 20, y, 4);
  y += 128 + 24;
  drawTextWrapped(renderer, WREN_GOLOS_56_HEADLINE_FONT_ID, 20, y, headline, width, titleLines, 53, true, -2);
  y += titleLines * 53 + 14;
  drawTextWrapped(renderer, WREN_GOLOS_18_FONT_ID, 20, y, body, width, bodyLines, 25);
  y += bodyLines * 25;
  if (hasReason) {
    y += 28;
    renderer.fillRect(20, y, width, 3, true);
    y += 3 + 12;
    drawLabel(renderer, 20, y, tr(STR_WREN_CRASH_REASON));
    y += 19 + 8;
    drawTextWrapped(renderer, WREN_MONO_16_FONT_ID, 20, y, reason, width, reasonLines, 22);
  }
  const Tab tabs[4] = {{TabKind::Empty}, {TabKind::Text, tab}, {TabKind::Empty}, {TabKind::Empty}};
  drawButtonTabs(renderer, tabs, false);
}

void drawNoCardScreen(GfxRenderer& renderer) {
  drawStateScreen(renderer, ui::icons::WrenNoCard, tr(STR_WREN_NOCARD_TITLE), tr(STR_WREN_NOCARD_BODY),
                  tr(STR_WREN_TAB_SELECT), 80);
}
}  // namespace wren::system
