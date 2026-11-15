#include "WrenSetupDisplayActivity.h"

#include <BoardConfig.h>

#include "CrossPointSettings.h"
#include "SetupState.h"
#include "SetupUi.h"
#include "wren/ui/WrenLayout.h"

void WrenSetupDisplayActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}
void WrenSetupDisplayActivity::close() {
  if (changed) SETTINGS.saveToFile();
  finish();
}
bool WrenSetupDisplayActivity::handleHomeGesture() {
  if (changed) SETTINGS.saveToFile();
  activityManager.goHome();
  return true;
}
void WrenSetupDisplayActivity::change(int delta) {
  using namespace wren::setup;
  switch (static_cast<DisplayRow>(selected)) {
    case DisplayRow::SleepScreen: {
      const int options[] = {CrossPointSettings::DARK, CrossPointSettings::COVER};
      SETTINGS.sleepScreen = options[cycle(sleepScreenIndex(SETTINGS.sleepScreen), delta, 2)];
      break;
    }
    case DisplayRow::QuickResume:
      SETTINGS.quickResumeSleepScreen = !SETTINGS.quickResumeSleepScreen;
      break;
    case DisplayRow::Battery:
      SETTINGS.hideBatteryPercentage = batteryIndex(SETTINGS.hideBatteryPercentage) == 0
                                           ? CrossPointSettings::HIDE_ALWAYS
                                           : CrossPointSettings::HIDE_NEVER;
      break;
    case DisplayRow::SunlightFix:
      SETTINGS.fadingFix = !SETTINGS.fadingFix;
      break;
    case DisplayRow::Count:
      return;
  }
  changed = true;
  requestUpdate();
}
void WrenSetupDisplayActivity::loop() {
  using B = MappedInputManager::Button;
  const int visible = std::max(0, (renderer.getScreenHeight() - 40 - 132) / 54);
  if (wren::setup::swipeSelection(mappedInput, selected, wren::setup::displayRowCount(BoardConfig::hasTouch()),
                                  visible)) {
    requestUpdate();
    return;
  }
  int touchedRow = 0;
  const auto touch = mappedInput.rowTouch(
      touchedRow, 132, 54, std::min(wren::setup::displayRowCount(BoardConfig::hasTouch()) - start, visible));
  if (touch != MappedInputManager::RowTouch::None) {
    selected = start + touchedRow;
    requestUpdate();
  }
  int tx = 0, ty = 0;
  const bool tapped = mappedInput.wasScreenTapped(tx, ty);
  if (mappedInput.wasReleased(B::Back) || mappedInput.wasBackGesture() || (tapped && ty >= 28 && ty < 68)) {
    close();
    return;
  }
  if (mappedInput.wasReleased(B::Confirm) || touch == MappedInputManager::RowTouch::Tap) {
    change(1);
    return;
  }
  const int count = wren::setup::displayRowCount(BoardConfig::hasTouch());
  navigator.onPressAndContinuous({B::Left}, [this, count] {
    selected = (selected + count - 1) % count;
    requestUpdate();
  });
  navigator.onPressAndContinuous({B::Right}, [this, count] {
    selected = (selected + 1) % count;
    requestUpdate();
  });
  navigator.onPressAndContinuous({B::Up}, [this] { change(-1); });
  navigator.onPressAndContinuous({B::Down}, [this] { change(1); });
}
void WrenSetupDisplayActivity::render(RenderLock&&) {
  using namespace wren::ui;
  using namespace wren::setup;
  status(renderer);
  clipContent(renderer);
  int y = drawHeader(renderer, 28, tr(STR_WREN_TAB_SETUP), tr(STR_WREN_SETUP_DISPLAY));
  const int count = displayRowCount(BoardConfig::hasTouch());
  const int visible = std::max(0, (renderer.getScreenHeight() - 40 - y) / layout::ROW_HEIGHT);
  start = scrollStart(start, selected, visible, count);
  const StrId labels[] = {StrId::STR_WREN_SETUP_SLEEP_SCREEN, StrId::STR_WREN_SETUP_QUICK_RESUME,
                          StrId::STR_WREN_SETUP_BATTERY, StrId::STR_WREN_SETUP_SUNLIGHT_FIX};
  for (int i = start; i < count && i < start + visible; ++i) {
    Row row;
    row.label = I18N.get(labels[i]);
    row.selected = selected == i;
    switch (static_cast<DisplayRow>(i)) {
      case DisplayRow::SleepScreen:
        row.marker = Marker::Options;
        row.optionCount = 2;
        row.chosenOption = sleepScreenIndex(SETTINGS.sleepScreen);
        row.value = row.chosenOption == 0   ? tr(STR_WREN_SLEEP_SCREEN_WREN)
                    : row.chosenOption == 1 ? tr(STR_WREN_SETUP_COVER)
                                            : "";
        break;
      case DisplayRow::QuickResume:
        row.marker = Marker::Toggle;
        row.on = SETTINGS.quickResumeSleepScreen;
        break;
      case DisplayRow::Battery:
        row.marker = Marker::Options;
        row.optionCount = 2;
        row.chosenOption = batteryIndex(SETTINGS.hideBatteryPercentage);
        row.value = row.chosenOption == 0 ? tr(STR_WREN_SETUP_SHOW) : tr(STR_WREN_SETUP_HIDE);
        break;
      case DisplayRow::SunlightFix:
        row.marker = Marker::Toggle;
        row.on = SETTINGS.fadingFix;
        break;
      case DisplayRow::Count:
        break;
    }
    y = drawRow(renderer, y, row);
  }
  unclip(renderer);
  footer(renderer);
  renderer.displayBuffer(cleanRefresh ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanRefresh = false;
}
