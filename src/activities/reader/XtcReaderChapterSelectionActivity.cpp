#include "XtcReaderChapterSelectionActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "wren/reader/ChaptersState.h"
#include "wren/reader/ReaderMenuState.h"
#include "wren/ui/WrenChrome.h"

XtcReaderChapterSelectionActivity::XtcReaderChapterSelectionActivity(GfxRenderer& r, MappedInputManager& input,
                                                                     const std::shared_ptr<Xtc>& book, uint32_t page)
    : Activity("XtcReaderChapterSelection", r, input), UiAppHost(r), xtc(book), currentPage(page) {}

int XtcReaderChapterSelectionActivity::visible() const {
  return std::max(1, (renderer.getScreenHeight() - 132 - 30) / 62);
}

void XtcReaderChapterSelectionActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  app.setScreen(&XtcReaderChapterSelectionActivity::buildHits, this);
  if (xtc) {
    title = xtc->getTitle();
    const auto& chapters = xtc->getChapters();
    const auto it = std::find_if(chapters.begin(), chapters.end(), [this](const auto& chapter) {
      return currentPage >= chapter.startPage && currentPage <= chapter.endPage;
    });
    if (it != chapters.end()) selected = static_cast<int>(it - chapters.begin());
    start = wren::reader::scrollStart(selected, start, visible());
  }
  requestUpdate();
}

void XtcReaderChapterSelectionActivity::move(int index) {
  const int count = xtc ? static_cast<int>(xtc->getChapters().size()) : 0;
  if (!count) return;
  selected = std::max(0, std::min(count - 1, index));
  start = wren::reader::scrollStart(selected, start, visible());
  requestUpdate();
}

void XtcReaderChapterSelectionActivity::activate(int index) {
  if (!xtc || index < 0 || index >= static_cast<int>(xtc->getChapters().size())) return;
  setResult(PageResult{xtc->getChapters()[index].startPage});
  finish();
}

void XtcReaderChapterSelectionActivity::loop() {
  using B = MappedInputManager::Button;
  RenderLock lock;
  if (mappedInput.wasReleased(B::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }
  if (mappedInput.wasReleased(B::Confirm)) {
    activate(selected);
    return;
  }
  const auto touch = routeTouch(mappedInput);
  if (touch.event && touch.event.action == 1) {
    app.clearTapFlash();
    activate(touch.event.value);
    return;
  }
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    const int count = xtc ? static_cast<int>(xtc->getChapters().size()) : 0;
    start = std::max(0, std::min(std::max(0, count - visible()),
                                 start + (swipe == MappedInputManager::SwipeDir::Up ? visible() : -visible())));
    requestUpdate();
    return;
  }
  const int count = xtc ? static_cast<int>(xtc->getChapters().size()) : 0;
  navigator.onNextPress([this, count] { move(ButtonNavigator::nextIndex(selected, count)); });
  navigator.onPreviousPress([this, count] { move(ButtonNavigator::previousIndex(selected, count)); });
  navigator.onNextContinuous([this, count] { move(ButtonNavigator::nextPageIndex(selected, count, visible())); });
  navigator.onPreviousContinuous(
      [this, count] { move(ButtonNavigator::previousPageIndex(selected, count, visible())); });
}

void XtcReaderChapterSelectionActivity::buildHits(UiScreen& screen, void* context) {
  auto& self = *static_cast<XtcReaderChapterSelectionActivity*>(context);
  const int count = self.xtc ? static_cast<int>(self.xtc->getChapters().size()) : 0;
  for (int i = self.start; i < count && i < self.start + self.visible(); ++i) {
    const freeink::ui::Rect row{0, static_cast<int16_t>(132 + (i - self.start) * 62),
                                static_cast<int16_t>(self.renderer.getScreenWidth()), 62};
    screen.frame().hit(row, 1, static_cast<int16_t>(i), freeink::ui::InputTouch);
  }
}

void XtcReaderChapterSelectionActivity::render(RenderLock&&) {
  using namespace wren::ui;
  renderer.clearScreen();
  char clock[16] = {};
  halClock.formatTime(clock, sizeof(clock), false);
  drawMenuStatusBar(renderer, clock, powerManager.getBatteryPercentage());
  int y = drawHeader(renderer, 28, title.c_str(), tr(STR_WREN_MENU_CHAPTERS));
  const int count = xtc ? static_cast<int>(xtc->getChapters().size()) : 0;
  char number[16], last[16], value[16];
  wren::reader::chapterNumber(last, sizeof(last), std::max(0, count - 1));
  const int numberWidth = std::max(28, labelWidth(renderer, last));
  for (int i = start; i < count && i < start + visible(); ++i) {
    const auto& chapter = xtc->getChapters()[i];
    wren::reader::chapterNumber(number, sizeof(number), i);
    snprintf(value, sizeof(value), "%d%%", wren::reader::chapterPercent(chapter.startPage, xtc->getPageCount()));
    drawChapterRow(renderer, y, number, chapter.name.empty() ? tr(STR_UNNAMED) : chapter.name.c_str(), value,
                   numberWidth, i == selected);
    y += 62;
  }
  const Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_TAB_BACK)},
                       {TabKind::Text, tr(STR_WREN_TAB_SELECT)},
                       {TabKind::ArrowUp},
                       {TabKind::ArrowDown}};
  renderUi();
  drawButtonTabs(renderer, tabs);
  drawEdgeTabs(renderer, true, true);
  renderer.displayBuffer();
}
