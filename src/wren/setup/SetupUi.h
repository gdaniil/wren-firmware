#pragma once
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "wren/ui/WrenChrome.h"

namespace wren::setup {
inline void status(const GfxRenderer& r) {
  r.clearScreen();
  char clock[16]{};
  halClock.formatTime(clock, sizeof(clock), false);
  wren::ui::drawMenuStatusBar(r, clock, powerManager.getBatteryPercentage());
}
inline void footer(GfxRenderer& r, bool edges = true) {
  using namespace wren::ui;
  const Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_TAB_BACK)},
                       {TabKind::Text, tr(STR_WREN_TAB_SELECT)},
                       {TabKind::ArrowUp},
                       {TabKind::ArrowDown}};
  drawButtonTabs(r, tabs, false);
  drawEdgeTabs(r, edges, edges, false);
}
// Setup lists and their hints share logical coordinates in every orientation.
inline void clipContent(const GfxRenderer& r) { r.setClipRect(0, 0, r.getScreenWidth(), r.getScreenHeight() - 40); }
// Restore the renderer's unbounded default; a screen-sized rect would cut off the next orientation.
inline void unclip(const GfxRenderer& r) { r.setClipRect(0, 0, 32767, 32767); }
inline bool swipeSelection(const MappedInputManager& input, int& selected, int count, int visible) {
  const auto swipe = input.wasSwipe();
  if (swipe != MappedInputManager::SwipeDir::Up && swipe != MappedInputManager::SwipeDir::Down) return false;
  const int delta = swipe == MappedInputManager::SwipeDir::Up ? std::max(1, visible) : -std::max(1, visible);
  selected = std::max(0, std::min(count - 1, selected + delta));
  return true;
}
inline int cycle(int index, int delta, int count) { return index < 0 ? 0 : (index + delta + count) % count; }
}  // namespace wren::setup
