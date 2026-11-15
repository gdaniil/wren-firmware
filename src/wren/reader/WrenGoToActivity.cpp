#include "WrenGoToActivity.h"

#include <Epub.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <cstdio>

#include "GoToState.h"
#include "wren/ui/WrenChrome.h"
#include "wren/ui/WrenFonts.h"

WrenGoToActivity::WrenGoToActivity(GfxRenderer& r, MappedInputManager& input, int initial, const std::string& t,
                                   std::shared_ptr<Epub> book)
    : Activity("WrenGoTo", r, input), percent(wren::reader::stepPercent(initial, 0)), title(t), epub(std::move(book)) {}
void WrenGoToActivity::onEnter() {
  Activity::onEnter();
  updateChapter();
  requestUpdate();
}
void WrenGoToActivity::updateChapter() {
  chapter.clear();
  if (!epub) return;
  int spine = wren::reader::percentSpine(percent, epub->getBookSize(), epub->getSpineItemsCount(),
                                         [this](int i) { return epub->getCumulativeSpineItemSize(i); });
  if (spine < 0) return;
  int toc = epub->getTocIndexForSpineIndex(spine);
  if (toc >= 0 && toc < epub->getTocItemsCount()) chapter = epub->getTocItem(toc).title;
  chapter = renderer.truncatedText(WREN_GOLOS_18_FONT_ID, chapter.c_str(), renderer.getScreenWidth() - 40);
}
void WrenGoToActivity::adjust(int delta) {
  int next = wren::reader::stepPercent(percent, delta);
  if (next == percent) return;
  percent = next;
  updateChapter();
  requestUpdate();
}
void WrenGoToActivity::cancel() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}
bool WrenGoToActivity::handleHomeGesture() {
  cancel();
  return true;
}
void WrenGoToActivity::loop() {
  using B = MappedInputManager::Button;
  RenderLock lock;
  if (mappedInput.wasReleased(B::Back)) {
    cancel();
    return;
  }
  if (mappedInput.wasReleased(B::Confirm)) {
    setResult(PercentResult{percent});
    finish();
    return;
  }
  navigator.onPressAndContinuous({B::PageBack}, [this] { adjust(-1); });
  navigator.onPressAndContinuous({B::PageForward}, [this] { adjust(1); });
  navigator.onPressAndContinuous({B::Left}, [this] { adjust(10); });
  navigator.onPressAndContinuous({B::Right}, [this] { adjust(-10); });
}
void WrenGoToActivity::render(RenderLock&&) {
  using namespace wren::ui;
  const int w = renderer.getScreenWidth(), h = renderer.getScreenHeight();
  renderer.clearScreen();
  char clock[16] = {};
  halClock.formatTime(clock, sizeof(clock), false);
  drawMenuStatusBar(renderer, clock, powerManager.getBatteryPercentage());
  int top = drawHeader(renderer, 28, title.c_str(), tr(STR_WREN_GOTO_TITLE));
  int y = top + std::max(0, (h - 80 - top - 325) / 2);
  char number[8];
  snprintf(number, sizeof(number), "%d%%", percent);
  int nw = renderer.getTextAdvanceX(WREN_GOLOS_160_DIGITS_FONT_ID, number, EpdFontFamily::REGULAR, -10);
  if (nw > w - 120) {
    snprintf(number, sizeof(number), "%d", percent);
    nw = renderer.getTextAdvanceX(WREN_GOLOS_160_DIGITS_FONT_ID, number, EpdFontFamily::REGULAR, -10);
  }
  drawIcon(renderer, icons::ChevronLeft, 20, y + 60, 4);
  drawIcon(renderer, icons::ChevronRight, w - 20 - icons::ChevronRight.width * 4, y + 60, 4);
  renderer.drawText(WREN_GOLOS_160_DIGITS_FONT_ID, (w - nw) / 2, y, number, true, EpdFontFamily::REGULAR,
                    BidiUtils::BidiBaseDir::AUTO, -10);
  y += 196;
  renderer.drawRect(20, y, w - 40, 20, 2, true);
  renderer.fillRect(22, y + 2, (w - 44) * percent / 100, 16, true);
  y += 26;
  for (int i = 0; i < 4; ++i) renderer.fillRect(20 + (w - 40) * i / 4, y, 3, 12, true);
  y += 20;
  drawLabel(renderer, 20, y, "0");
  drawLabel(renderer, (w - labelWidth(renderer, "50")) / 2, y, "50");
  drawLabel(renderer, w - 20 - labelWidth(renderer, "100"), y, "100");
  y += 48;
  renderer.fillRect(20, y, w - 40, 3, true);
  y += 19;
  renderer.drawText(WREN_GOLOS_18_FONT_ID, 20, y, chapter.c_str());
  const char* hint = tr(STR_WREN_GOTO_HINT);
  drawLabel(renderer, (w - labelWidth(renderer, hint)) / 2, h - 54, hint);
  const Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_TAB_BACK)},
                       {TabKind::Text, tr(STR_WREN_TAB_SELECT)},
                       {TabKind::ArrowUp},
                       {TabKind::ArrowDown}};
  drawButtonTabs(renderer, tabs);
  drawEdgeTabs(renderer, true, true);
  renderer.displayBuffer(first ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  first = false;
}
