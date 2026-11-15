#include "WrenReaderMenuActivity.h"

#include <Arduino.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include "ReaderMenuState.h"
#include "activities/reader/ReaderUtils.h"
#include "wren/ui/WrenChrome.h"
#include "wren/ui/WrenLayout.h"

using Action = EpubReaderMenuActivity::MenuAction;
namespace {
constexpr Action actions[] = {Action::TEXT_SETTINGS, Action::SELECT_CHAPTER, Action::TOGGLE_BOOKMARK, Action::BOOKMARKS,
                              Action::GO_TO_PERCENT, Action::ROTATE_SCREEN,  Action::DELETE_CACHE,    Action::GO_HOME};
constexpr StrId labels[] = {StrId::STR_WREN_MENU_READER_SETTINGS, StrId::STR_WREN_MENU_CHAPTERS,
                            StrId::STR_WREN_MENU_ADD_BOOKMARK,    StrId::STR_WREN_MENU_BOOKMARKS,
                            StrId::STR_WREN_MENU_GO_TO,           StrId::STR_WREN_MENU_ORIENTATION,
                            StrId::STR_WREN_MENU_REFRESH,         StrId::STR_WREN_MENU_GO_HOME};
}  // namespace
WrenReaderMenuActivity::WrenReaderMenuActivity(GfxRenderer& r, MappedInputManager& input, const std::string& t, int p,
                                               int n, int progress, uint8_t o, size_t count, Action& selection,
                                               bool message)
    : Activity("WrenReaderMenu", r, input),
      selectedAction(selection),
      title(t),
      page(p),
      total(n),
      percent(progress),
      orientation(o),
      bookmarks(count),
      added(message) {}
void WrenReaderMenuActivity::onEnter() {
  Activity::onEnter();
  for (int i = 0; i < 8; ++i)
    if (actions[i] == selectedAction) selected = i;
  addedAt = millis();
  requestUpdate();  // the activity manager does not paint a new activity by itself
}
void WrenReaderMenuActivity::onExit() {
  selectedAction = actions[selected];
  Activity::onExit();
}
void WrenReaderMenuActivity::close(bool cancelled) {
  ActivityResult result;
  result.isCancelled = cancelled;
  result.data = MenuResult{cancelled ? -1 : static_cast<int>(actions[selected]), orientation, 0};
  setResult(std::move(result));
  finish();
}
bool WrenReaderMenuActivity::handleHomeGesture() {
  close(true);
  return true;
}
void WrenReaderMenuActivity::loop() {
  using Button = MappedInputManager::Button;
  RenderLock lock;
  uint32_t now = millis();
  if (added && !wren::reader::within(now, addedAt, 1500)) {
    added = false;
    requestUpdate();
  }
  if (confirming && !wren::reader::within(now, confirmAt, 3000)) {
    confirming = false;
    requestUpdate();
  }
  if (mappedInput.wasReleased(Button::Back)) {
    lock.unlock();
    close(true);
    return;
  }
  if (mappedInput.wasReleased(Button::Confirm)) {
    if (actions[selected] == Action::ROTATE_SCREEN) {
      orientation = wren::reader::toggleOrientation(orientation);
      ReaderUtils::applyOrientation(renderer, orientation);
      requestUpdate(true);
    } else if (actions[selected] == Action::DELETE_CACHE && !confirming) {
      confirming = true;
      confirmAt = now;
      requestUpdate();
    } else {
      lock.unlock();
      close(false);
    }
    return;
  }
  int delta = (mappedInput.wasPressed(Button::Right) || mappedInput.wasPressed(Button::PageForward)) ? 1
              : (mappedInput.wasPressed(Button::Left) || mappedInput.wasPressed(Button::PageBack))   ? -1
                                                                                                     : 0;
  if (delta) {
    selected = (selected + delta + 8) % 8;
    confirming = false;
    requestUpdate();
  }
}
void WrenReaderMenuActivity::render(RenderLock&&) {
  using namespace wren::ui;
  renderer.clearScreen();
  char clock[16] = {};
  halClock.formatTime(clock, sizeof(clock), false);
  drawMenuStatusBar(renderer, clock, powerManager.getBatteryPercentage());
  int y = drawBookHeader(renderer, 28, title.c_str(), page, total, percent);
  int visible = std::max(1, (renderer.getScreenHeight() - y - 30) / wren::ui::layout::ROW_HEIGHT);
  start = wren::reader::scrollStart(selected, start, visible);
  for (int i = start; i < 8 && i < start + visible; ++i) {
    Row row;
    row.label = I18N.get(labels[i]);
    row.selected = selected == i;
    if (i == 2 && added) row.label = tr(STR_WREN_MENU_BOOKMARK_ADDED);
    if (i == 6 && confirming) row.label = tr(STR_WREN_MENU_REFRESH_CONFIRM);
    char count[24];
    if (i == 3) {
      snprintf(count, sizeof(count), "%u", static_cast<unsigned>(bookmarks));
      row.value = count;
    }
    if (i == 0 || i == 1 || i == 3 || i == 4) row.marker = Marker::Chevron;
    if (i == 2 || i == 6 || i == 7) {
      row.marker = Marker::Icon;
      row.icon = i == 2 ? &icons::Bookmark : i == 6 ? &icons::Refresh : &icons::Home;
      row.iconScale = i == 6 ? 1 : 2;
    }
    if (i == 5) {
      bool land = wren::reader::landscape(orientation);
      row.value = land ? tr(STR_WREN_LANDSCAPE) : tr(STR_WREN_PORTRAIT);
      row.marker = Marker::Options;
      row.optionCount = 2;
      row.chosenOption = land ? 1 : 0;
    }
    y = drawRow(renderer, y, row);
  }
  const Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_TAB_CLOSE)},
                       {TabKind::Text, tr(STR_WREN_TAB_SELECT)},
                       {TabKind::ArrowUp},
                       {TabKind::ArrowDown}};
  drawButtonTabs(renderer, tabs);
  renderer.displayBuffer();
}
