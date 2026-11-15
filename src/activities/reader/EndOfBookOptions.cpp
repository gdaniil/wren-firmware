#include "EndOfBookOptions.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <algorithm>
#include <utility>

#include "CrossPointSettings.h"
#include "HapticFeedback.h"
#include "ReaderUtils.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"
#include "wren/ui/WrenChrome.h"
#include "wren/ui/WrenFonts.h"
#include "wren/ui/WrenLayout.h"

namespace {
constexpr int ROW_COUNT = 3;
struct HeroLayout {
  int scale;
  int top;
  int lines;
  int rowTop;
};

HeroLayout heroLayout(const GfxRenderer& renderer) {
  using namespace wren::ui;
  const int height = renderer.getScreenHeight();
  const int width = renderer.getScreenWidth() - 40;
  const int wanted =
      std::max(1, wrappedLineCount(renderer, WREN_GOLOS_56_HEADLINE_FONT_ID, tr(STR_WREN_END_TITLE), width, 2, -2));
  // Reserve status, hero gaps, subtitle, rule, three 54px rows and 40px tabs.
  const int fixed = 28 + 20 + 14 + 25 + 28 + 3 + ROW_COUNT * layout::ROW_HEIGHT + 40;
  const bool compact = fixed + 40 + 128 + wanted * 53 > height;
  const int scale = compact ? 2 : 4;
  const int top = compact ? 20 : 40;
  const int availableLines = std::max(1, (height - fixed - top - 32 * scale) / 53);
  const int lines = std::min(wanted, availableLines);
  return {scale, top, lines, 28 + top + 32 * scale + 20 + lines * 53 + 14 + 25 + 28 + 3};
}
}  // namespace

EndOfBookOptions::EndOfBookOptions(GfxRenderer& renderer, std::string title, std::string author)
    : renderer(renderer), title(std::move(title)), subtitle(this->title), rowTop(heroLayout(renderer).rowTop) {
  if (!author.empty()) subtitle += " · " + author;
}

EndOfBookOptions::Action EndOfBookOptions::selectedAction(int selected) const {
  switch (selected) {
    case 0:
      return Action::GoHome;
    case 1:
      return Action::Restart;
    case 2:
      return Action::Delete;
  }
  return Action::None;
}

EndOfBookOptions::Action EndOfBookOptions::handleMenuInput(const MappedInputManager& input) {
  int tappedRow = -1;
  const auto touch = input.rowTouch(tappedRow, rowTop.load(std::memory_order_acquire), wren::ui::layout::ROW_HEIGHT,
                                    ROW_COUNT, 0, renderer.getScreenWidth());
  if (touch == MappedInputManager::RowTouch::Tap) {
    haptic_feedback::touchAction();
    return selectedAction(tappedRow);
  }
  if (touch == MappedInputManager::RowTouch::Down && tappedRow != selector.load(std::memory_order_relaxed)) {
    selector.store(tappedRow, std::memory_order_relaxed);
    return Action::Redraw;
  }
  const int selected = selector.load(std::memory_order_relaxed);
  if (input.wasReleased(MappedInputManager::Button::Confirm)) return selectedAction(selected);
  if (input.wasReleased(MappedInputManager::Button::Back) && input.getHeldTime() < ReaderUtils::GO_HOME_MS)
    return Action::LastPage;
  const bool usePress = SETTINGS.longPressButtonBehavior == CrossPointSettings::OFF;
  const auto triggered = [&](MappedInputManager::Button button) {
    return usePress ? input.wasPressed(button) : input.wasReleased(button);
  };
  if (triggered(MappedInputManager::Button::NavPrevious)) {
    selector.store(ButtonNavigator::previousIndex(selected, ROW_COUNT), std::memory_order_relaxed);
    return Action::Redraw;
  }
  if (triggered(MappedInputManager::Button::NavNext)) {
    selector.store(ButtonNavigator::nextIndex(selected, ROW_COUNT), std::memory_order_relaxed);
    return Action::Redraw;
  }
  return Action::None;
}

void EndOfBookOptions::moveSelection(bool forward) {
  const int selected = selector.load(std::memory_order_relaxed);
  selector.store(
      forward ? ButtonNavigator::nextIndex(selected, ROW_COUNT) : ButtonNavigator::previousIndex(selected, ROW_COUNT),
      std::memory_order_relaxed);
}

void EndOfBookOptions::render(GfxRenderer& renderer, const MappedInputManager& input) {
  using namespace wren::ui;
  (void)input;
  char clock[16] = {};
  halClock.formatTime(clock, sizeof(clock), false);
  drawMenuStatusBar(renderer, clock, powerManager.getBatteryPercentage());
  const auto hero = heroLayout(renderer);
  int y = 28 + hero.top;
  drawIcon(renderer, icons::WrenDone, 20, y, hero.scale);
  y += 32 * hero.scale + 20;
  drawTextWrapped(renderer, WREN_GOLOS_56_HEADLINE_FONT_ID, 20, y, tr(STR_WREN_END_TITLE),
                  renderer.getScreenWidth() - 40, hero.lines, 53, true, -2);
  y += hero.lines * 53 + 14;
  drawTextFit(renderer, WREN_GOLOS_18_FONT_ID, 20, y, subtitle.c_str(), renderer.getScreenWidth() - 40);
  y += 25 + 28;
  renderer.fillRect(0, y, renderer.getScreenWidth(), 3, true);
  y += 3;
  const char* labels[] = {tr(STR_WREN_END_LIBRARY), tr(STR_WREN_END_RESTART), tr(STR_WREN_LIB_DELETE)};
  const int selected = selector.load(std::memory_order_relaxed);
  for (int i = 0; i < ROW_COUNT; ++i) {
    Row row;
    row.label = labels[i];
    row.selected = selected == i;
    if (i == 0) row.marker = Marker::Chevron;
    if (i == 2) row.leadingIcon = &icons::Trash;
    y = drawRow(renderer, y, row);
  }
  const Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_TAB_BACK)},
                       {TabKind::Text, tr(STR_WREN_TAB_SELECT)},
                       {TabKind::ArrowUp},
                       {TabKind::ArrowDown}};
  drawButtonTabs(renderer, tabs);
  rowTop.store(hero.rowTop, std::memory_order_release);
}
