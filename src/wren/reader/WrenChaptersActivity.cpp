#include "WrenChaptersActivity.h"

#include <Epub.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "ChaptersState.h"
#include "ReaderMenuState.h"
#include "wren/ui/WrenChrome.h"
WrenChaptersActivity::WrenChaptersActivity(GfxRenderer& r, MappedInputManager& i, const std::shared_ptr<Epub>& e,
                                           int current)
    : Activity("WrenChapters", r, i), epub(e), selected(std::max(0, current)) {}
int WrenChaptersActivity::visible() const { return std::max(1, (renderer.getScreenHeight() - 132 - 30) / 62); }
void WrenChaptersActivity::onEnter() {
  Activity::onEnter();
  selected = std::min(selected, std::max(0, epub ? epub->getTocItemsCount() - 1 : 0));
  start = wren::reader::scrollStart(selected, start, visible());
  requestUpdate();
}
void WrenChaptersActivity::cancel() {
  ActivityResult r;
  r.isCancelled = true;
  setResult(std::move(r));
  finish();
}
bool WrenChaptersActivity::handleHomeGesture() {
  cancel();
  return true;
}
void WrenChaptersActivity::move(int delta, bool wrap) {
  int count = std::max(1, epub ? epub->getTocItemsCount() : 0);
  int next = wrap ? (selected + delta + count) % count : wren::reader::chapterPageJump(selected, delta, count);
  if (next != selected) {
    selected = next;
    start = wren::reader::scrollStart(selected, start, visible());
    requestUpdate();
  }
}
void WrenChaptersActivity::loop() {
  using B = MappedInputManager::Button;
  if (mappedInput.wasReleased(B::Back)) {
    cancel();
    return;
  }
  if (mappedInput.wasReleased(B::Confirm)) {
    if (!epub) {
      cancel();
      return;
    }
    if (!epub->getTocItemsCount()) {
      setResult(ChapterResult{0, ""});
      finish();
      return;
    }
    // The metadata file has a shared seek position with render().
    auto item = [this] {
      RenderLock lock;
      return epub->getTocItem(selected);
    }();
    if (item.spineIndex < 0) {
      cancel();
      return;
    }
    setResult(ChapterResult{item.spineIndex, item.anchor});
    finish();
    return;
  }
  navigator.onPressAndContinuous({B::Left}, [this] { move(-1, true); });
  navigator.onPressAndContinuous({B::Right}, [this] { move(1, true); });
  navigator.onPressAndContinuous({B::PageBack}, [this] { move(-visible(), false); });
  navigator.onPressAndContinuous({B::PageForward}, [this] { move(visible(), false); });
}
void WrenChaptersActivity::render(RenderLock&&) {
  using namespace wren::ui;
  renderer.clearScreen();
  char clock[16] = {};
  halClock.formatTime(clock, sizeof(clock), false);
  drawMenuStatusBar(renderer, clock, powerManager.getBatteryPercentage());
  int y = drawHeader(renderer, 28, epub ? epub->getTitle().c_str() : "", tr(STR_WREN_MENU_CHAPTERS));
  int count = epub ? epub->getTocItemsCount() : 0;
  char number[16], value[16], last[16];
  wren::reader::chapterNumber(last, sizeof(last), std::max(0, count - 1));
  int numberWidth = std::max(28, labelWidth(renderer, last));
  for (int i = start; i < std::max(1, count) && i < start + visible(); ++i) {
    wren::reader::chapterNumber(number, sizeof(number), i);
    if (!count) {
      drawChapterRow(renderer, y, number, tr(STR_WREN_CHAPTERS_WHOLE_BOOK), "0%", numberWidth, i == selected);
    } else {
      auto item = epub->getTocItem(i);
      if (item.spineIndex < 0)
        snprintf(value, sizeof(value), "—");
      else
        snprintf(value, sizeof(value), "%d%%",
                 wren::reader::chapterPercent(
                     item.spineIndex ? epub->getCumulativeSpineItemSize(item.spineIndex - 1) : 0, epub->getBookSize()));
      drawChapterRow(renderer, y, number, item.title.c_str(), value, numberWidth, i == selected);
    }
    y += 62;
  }
  const Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_TAB_BACK)},
                       {TabKind::Text, tr(STR_WREN_TAB_SELECT)},
                       {TabKind::ArrowUp},
                       {TabKind::ArrowDown}};
  drawButtonTabs(renderer, tabs);
  drawEdgeTabs(renderer, true, true);
  renderer.displayBuffer(first ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  first = false;
}
