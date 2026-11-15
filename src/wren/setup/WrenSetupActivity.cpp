#include "WrenSetupActivity.h"

#include <Memory.h>

#include "SetupState.h"
#include "SetupUi.h"
#include "WrenFilesActivity.h"
#include "WrenSetupDisplayActivity.h"
#include "WrenSetupSystemActivity.h"
#include "wren/reader/WrenReaderSettingsActivity.h"
#include "wren/ui/WrenFonts.h"

void WrenSetupActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}
bool WrenSetupActivity::handleHomeGesture() {
  activityManager.goHome();
  return true;
}
void WrenSetupActivity::open() {
  using namespace wren::setup;
  std::unique_ptr<Activity> child;
  // The manager owns one child; the small setup parent remains until the child returns.
  switch (static_cast<RootRow>(selected)) {
    case RootRow::Reader:
      child = makeUniqueNoThrow<WrenReaderSettingsActivity>(renderer, mappedInput, tr(STR_WREN_TAB_SETUP));
      break;
    case RootRow::Display:
      child = makeUniqueNoThrow<WrenSetupDisplayActivity>(renderer, mappedInput);
      break;
    case RootRow::System:
      child = makeUniqueNoThrow<WrenSetupSystemActivity>(renderer, mappedInput);
      break;
    case RootRow::Files:
      child = makeUniqueNoThrow<WrenFilesActivity>(renderer, mappedInput, "/");
      break;
    case RootRow::Count:
      return;
  }
  if (!child) {
    LOG_ERR("SETUP", "OOM: setup child activity");
    return;
  }
  const auto orientation = renderer.getOrientation();
  startActivityForResult(std::move(child), [this, orientation](const ActivityResult&) {
    RenderLock lock;
    renderer.setOrientation(orientation);
    cleanRefresh = true;
  });
}
void WrenSetupActivity::loop() {
  using B = MappedInputManager::Button;
  const int visible = std::max(0, (renderer.getScreenHeight() - 40 - 108) / 101);
  if (wren::setup::swipeSelection(mappedInput, selected, 4, visible)) {
    requestUpdate();
    return;
  }
  int touchedRow = 0;
  const auto touch = mappedInput.rowTouch(touchedRow, 108, 101, std::min(4 - start, visible));
  if (touch != MappedInputManager::RowTouch::None) {
    selected = start + touchedRow;
    requestUpdate();
  }
  int tx = 0, ty = 0;
  const bool tapped = mappedInput.wasScreenTapped(tx, ty);
  if (tapped && ty >= 40 && ty < 96 && tx >= 100 && tx < renderer.getScreenWidth() - 20) {
    const int tab = (tx - 100) * 3 / (renderer.getScreenWidth() - 120);
    if (tab == 0) activityManager.goHome();
    if (tab == 1) activityManager.goToFileTransfer();
    return;
  }
  if (mappedInput.wasReleased(B::Back) || mappedInput.wasBackGesture()) {
    activityManager.goHome();
    return;
  }
  if (mappedInput.wasReleased(B::Confirm) || touch == MappedInputManager::RowTouch::Tap) {
    open();
    return;
  }
  if (mappedInput.wasPressed(B::Up)) {
    activityManager.goToFileTransfer();
    return;
  }
  if (mappedInput.wasPressed(B::Down)) {
    activityManager.goHome();
    return;
  }
  navigator.onPressAndContinuous({B::Left}, [this] {
    selected = (selected + 3) % 4;
    requestUpdate();
  });
  navigator.onPressAndContinuous({B::Right}, [this] {
    selected = (selected + 1) % 4;
    requestUpdate();
  });
}
void WrenSetupActivity::render(RenderLock&&) {
  using namespace wren::ui;
  using namespace wren::setup;
  status(renderer);
  clipContent(renderer);
  drawTabHeader(renderer, 28, 2);
  constexpr int top = 108, rowHeight = 101;
  const int visible = std::max(0, (renderer.getScreenHeight() - 40 - top) / rowHeight);
  start = scrollStart(start, selected, visible, 4);
  const StrId labels[] = {StrId::STR_WREN_SETUP_READER, StrId::STR_WREN_SETUP_DISPLAY, StrId::STR_WREN_SETUP_SYSTEM,
                          StrId::STR_WREN_SETUP_FILES};
  for (int i = start; i < 4 && i < start + visible; ++i) {
    const int y = top + (i - start) * rowHeight;
    const bool ink = selected != i;
    renderer.fillRect(0, y, renderer.getScreenWidth(), rowHeight - 3, !ink);
    drawTextFit(renderer, WREN_GOLOS_40_FONT_ID, 20, y + 24, I18N.get(labels[i]), renderer.getScreenWidth() - 62, ink,
                -1);
    drawIcon(renderer, icons::ChevronRight, renderer.getScreenWidth() - 26, y + 43, 2, ink);
    renderer.fillRect(0, y + rowHeight - 3, renderer.getScreenWidth(), 3, true);
  }
  unclip(renderer);
  footer(renderer);
  renderer.displayBuffer(cleanRefresh ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanRefresh = false;
}
