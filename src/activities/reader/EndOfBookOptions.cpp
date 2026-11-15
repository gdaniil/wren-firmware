#include "EndOfBookOptions.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "HapticFeedback.h"
#include "ReaderUtils.h"
// ReaderUtils.h pulls in ActivityManager.h, which only forward-declares Activity while holding
// std::unique_ptr<Activity> members. Destroying that unique_ptr needs the complete type, so the
// definition must be visible here.
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"
#include "util/NextBookFinder.h"
#include "wren/ui/WrenChrome.h"
#include "wren/ui/WrenLayout.h"

namespace {
constexpr int HEADER_TOP = 28;
// drawHeader returns its top plus 104; touch rows must use the same origin.
constexpr int ROW_TOP = HEADER_TOP + 104;
constexpr int LABEL_HEIGHT = 30;

// Display name without the file extension, mirroring the file browser rows
std::string displayName(const std::string& filename) {
  const auto pos = filename.rfind('.');
  return filename.substr(0, pos);
}
}  // namespace

EndOfBookOptions::EndOfBookOptions(GfxRenderer& renderer) : renderer(renderer) {}

void EndOfBookOptions::loadOnce(const std::string& currentBookPath) {
  if (isLoaded.load(std::memory_order_acquire)) {
    return;
  }
  folder = FsHelpers::extractFolderPath(currentBookPath);
  names = NextBookFinder::findNextBooks(currentBookPath, MAX_SUGGESTIONS);
  const auto slash = currentBookPath.find_last_of('/');
  title = displayName(currentBookPath.substr(slash == std::string::npos ? 0 : slash + 1));
  for (size_t i = 0; i < names.size(); ++i) rowLabels[i] = displayName(names[i]);
  selector.store(0, std::memory_order_relaxed);
  isLoaded.store(true, std::memory_order_release);
}

bool EndOfBookOptions::menuActive() const { return isLoaded.load(std::memory_order_acquire); }

bool EndOfBookOptions::hasSuggestions() const { return isLoaded.load(std::memory_order_acquire) && !names.empty(); }

std::string EndOfBookOptions::fullPath(const size_t index) const {
  if (index >= names.size()) {
    return {};
  }
  return folder == "/" ? "/" + names[index] : folder + "/" + names[index];
}

EndOfBookOptions::Action EndOfBookOptions::handleMenuInput(const MappedInputManager& input, std::string* openPath) {
  int tappedRow = -1;
  const int top = ROW_TOP + (names.empty() ? 0 : LABEL_HEIGHT);
  const auto touch = input.rowTouch(tappedRow, top, wren::ui::layout::ROW_HEIGHT, static_cast<int>(names.size()) + 1, 0,
                                    renderer.getScreenWidth());
  if (touch == MappedInputManager::RowTouch::Tap) {
    haptic_feedback::touchAction();
    if (tappedRow < static_cast<int>(names.size())) {
      if (openPath) *openPath = fullPath(tappedRow);
      return Action::OpenBook;
    }
    return Action::GoHome;
  }
  if (touch == MappedInputManager::RowTouch::Down && tappedRow != selector.load(std::memory_order_relaxed)) {
    selector.store(tappedRow, std::memory_order_relaxed);
    return Action::Redraw;
  }

  const int selectedIndex = selector.load(std::memory_order_relaxed);
  if (input.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectedIndex < static_cast<int>(names.size())) {
      if (openPath) {
        *openPath = fullPath(selectedIndex);
      }
      return Action::OpenBook;
    }
    return Action::GoHome;  // "Home" entry selected
  }

  // Short-press Back returns to the last page; a long press falls through to the
  // reader's own handler (file browser). Home is reached through the list's Home entry.
  if (input.wasReleased(MappedInputManager::Button::Back) && input.getHeldTime() < ReaderUtils::GO_HOME_MS) {
    return Action::LastPage;
  }

  // With no suggestions, page turns (buttons, tilt, touch incl. RTL) stay with the reader:
  // handleEndOfBookPageTurn() maps next to Home and previous to the last page.
  if (names.empty()) return Action::None;

  // Selection movement on the standard list navigation buttons (side Up/Down plus front
  // Left/Right, orientation swap included). It follows the reader's page-turn semantics
  // (press-triggered by default, release-triggered when a long-press behavior is
  // configured, same rule as ReaderUtils::detectPageTurn). This matters on entry: with
  // press-triggered turns, the press that turned the final page already fired in the
  // reader, and its release must not double-fire into this menu.
  const bool usePress = SETTINGS.longPressButtonBehavior == CrossPointSettings::OFF;
  const auto triggered = [&](const MappedInputManager::Button button) {
    return usePress ? input.wasPressed(button) : input.wasReleased(button);
  };
  const int itemCount = static_cast<int>(names.size()) + 1;  // + "Home" entry
  if (triggered(MappedInputManager::Button::NavPrevious)) {
    selector.store(ButtonNavigator::previousIndex(selectedIndex, itemCount), std::memory_order_relaxed);
    return Action::Redraw;
  }
  if (triggered(MappedInputManager::Button::NavNext)) {
    selector.store(ButtonNavigator::nextIndex(selectedIndex, itemCount), std::memory_order_relaxed);
    return Action::Redraw;
  }
  return Action::None;
}

void EndOfBookOptions::render(GfxRenderer& renderer, const MappedInputManager& input) {
  using namespace wren::ui;
  (void)input;
  char clock[16] = {};
  halClock.formatTime(clock, sizeof(clock), false);
  drawMenuStatusBar(renderer, clock, powerManager.getBatteryPercentage());
  int y = drawHeader(renderer, HEADER_TOP, title.c_str(), tr(STR_WREN_END_TITLE));
  if (!names.empty()) {
    drawLabel(renderer, 20, y + 6, tr(STR_WREN_END_NEXT));
    y += LABEL_HEIGHT;
  }
  const int selected = selector.load(std::memory_order_relaxed);
  for (size_t i = 0; i <= names.size(); ++i) {
    Row row;
    row.label = i < names.size() ? rowLabels[i].c_str() : tr(STR_WREN_MENU_GO_HOME);
    row.selected = selected == static_cast<int>(i);
    if (i == names.size()) {
      row.marker = Marker::Icon;
      row.icon = &icons::Home;
      row.iconScale = 2;
    }
    y = drawRow(renderer, y, row);
  }
  const Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_TAB_BACK)},
                       {TabKind::Text, tr(STR_WREN_TAB_SELECT)},
                       {TabKind::ArrowUp},
                       {TabKind::ArrowDown}};
  drawButtonTabs(renderer, tabs);
}
