#include "WrenBookmarksActivity.h"

#include <Epub.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "BookmarksState.h"
#include "CrossPointSettings.h"
#include "util/BookmarkFile.h"
#include "wren/ui/WrenChrome.h"
#include "wren/ui/WrenFonts.h"

namespace {
constexpr int BODY_TOP = 132;
constexpr int BODY_BOTTOM_MARGIN = 78;
constexpr int ROW_TOP = 16;
constexpr int ROW_BOTTOM = 20;
constexpr int EXCERPT_GAP = 10;
constexpr int RULE = 2;
constexpr int LABEL_HEIGHT = 16;
}  // namespace

WrenBookmarksActivity::WrenBookmarksActivity(GfxRenderer& r, MappedInputManager& input,
                                             const std::shared_ptr<Epub>& book, const std::string& path)
    : Activity("WrenBookmarks", r, input), epub(book), epubPath(path) {}

int WrenBookmarksActivity::bodyHeight() const {
  return std::max(1, renderer.getScreenHeight() - BODY_BOTTOM_MARGIN - BODY_TOP);
}

void WrenBookmarksActivity::onEnter() {
  Activity::onEnter();
  if (epub) BookmarkFile::load(epubPath, bookmarks);
  font = SETTINGS.getReaderFontId();
  if (!font || !renderer.ensureFontLoaded(font)) font = WREN_GOLOS_18_FONT_ID;
  lineHeight = std::max(1, renderer.getLineHeight(font));
  if (ROW_TOP + LABEL_HEIGHT + EXCERPT_GAP + lineHeight + ROW_BOTTOM + RULE > bodyHeight()) {
    font = WREN_GOLOS_18_FONT_ID;
    lineHeight = std::max(1, renderer.getLineHeight(font));
  }
  const int capacity = bodyHeight() / (ROW_TOP + LABEL_HEIGHT + ROW_BOTTOM + RULE) + 2;
  rows.reserve(capacity);
  heights.reserve(capacity);
  rebuildWindow();
  requestUpdate();
}

WrenBookmarksActivity::Row WrenBookmarksActivity::makeRow(int index) const {
  using namespace wren::ui;
  const auto& bookmark = bookmarks[index];
  int toc = -1;
  if (epub && bookmark.computedSpineIndex < epub->getSpineItemsCount()) {
    toc = epub->getTocIndexForSpineIndex(bookmark.computedSpineIndex);
    if (toc >= epub->getTocItemsCount()) toc = -1;
  }
  char meta[240];
  wren::reader::bookmarkMeta(meta, sizeof(meta), tr(STR_WREN_BOOKMARK_META), bookmark.percentage, toc,
                             bookmark.name.c_str());
  // Fit the label without splitting a UTF-8 sequence.
  while (labelWidth(renderer, meta) > renderer.getScreenWidth() - 40) {
    size_t n = strlen(meta);
    if (!n) break;
    do {
      --n;
    } while (n && (static_cast<unsigned char>(meta[n]) & 0xc0) == 0x80);
    meta[n] = '\0';
  }
  Row row;
  row.meta = meta;
  const int available = bodyHeight() - ROW_TOP - LABEL_HEIGHT - EXCERPT_GAP - ROW_BOTTOM - RULE;
  const int maxLines = std::max(0, std::min(2, available / lineHeight));
  if (maxLines)
    row.lines = renderer.wrappedText(font, bookmark.summary.c_str(), renderer.getScreenWidth() - 40, maxLines);
  row.height = ROW_TOP + LABEL_HEIGHT + (row.lines.empty() ? 0 : EXCERPT_GAP + lineHeight * row.lines.size()) +
               ROW_BOTTOM + RULE;
  return row;
}

void WrenBookmarksActivity::rebuildWindow() {
  rows.clear();
  heights.clear();
  if (bookmarks.empty()) {
    start = selected = 0;
    return;
  }
  selected = std::clamp(selected, 0, static_cast<int>(bookmarks.size()) - 1);
  start = std::clamp(start, 0, selected);
  int used = 0;
  for (int i = start; i < static_cast<int>(bookmarks.size()); ++i) {
    Row row = makeRow(i);
    if (i > selected && used + row.height > bodyHeight()) break;
    used += row.height;
    heights.push_back(row.height);
    rows.push_back(std::move(row));
    if (i <= selected) {
      const int skip = wren::reader::bookmarkScrollStart(heights.data(), heights.size(), i - start, 0, bodyHeight());
      for (int j = 0; j < skip; ++j) used -= heights[j];
      if (skip) {
        rows.erase(rows.begin(), rows.begin() + skip);
        heights.erase(heights.begin(), heights.begin() + skip);
        start += skip;
      }
    }
  }
}

void WrenBookmarksActivity::cancel() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}

bool WrenBookmarksActivity::handleHomeGesture() {
  cancel();
  return true;
}

void WrenBookmarksActivity::openSelectedBookmark() {
  if (bookmarks.empty() || !epub) return;
  const auto& bookmark = bookmarks.at(selected);
  ProgressChangeResult result{};
  result.xpath = bookmark.xpath;
  result.percentage = bookmark.percentage;
  result.hasSavedProgress = true;
  result.hasVisibleTextOffset = bookmark.hasVisibleTextOffset;
  result.visibleTextOffset = bookmark.visibleTextOffset;
  result.spineIndex = bookmark.computedSpineIndex;
  if (bookmark.computedChapterPageCount > 0 && bookmark.computedChapterProgress < bookmark.computedChapterPageCount &&
      bookmark.computedSpineIndex < epub->getSpineItemsCount()) {
    result.page = bookmark.computedChapterProgress;
    result.totalPages = bookmark.computedChapterPageCount;
  }
  setResult(std::move(result));
  finish();
}

void WrenBookmarksActivity::deleteSelectedBookmark() {
  if (bookmarks.empty()) return;
  bookmarks.erase(bookmarks.begin() + selected);
  if (!BookmarkFile::save(epubPath, bookmarks)) LOG_ERR("WBM", "Failed to save bookmarks after delete");
  rebuildWindow();
  requestUpdate();
}

void WrenBookmarksActivity::move(int delta) {
  if (bookmarks.empty()) return;
  int next = std::clamp(selected + delta, 0, static_cast<int>(bookmarks.size()) - 1);
  if (next == selected) return;
  selected = next;
  if (selected < start || selected >= start + static_cast<int>(rows.size())) rebuildWindow();
  requestUpdate();
}

void WrenBookmarksActivity::loop() {
  using B = MappedInputManager::Button;
  RenderLock lock;
  if (mappedInput.wasLongPressed(B::Back, 1000)) {
    deleteSelectedBookmark();
    return;
  }
  if (mappedInput.wasReleased(B::Back)) {
    cancel();
    return;
  }
  if (mappedInput.wasReleased(B::Confirm)) {
    openSelectedBookmark();
    return;
  }
  navigator.onPressAndContinuous({B::Left}, [this] { move(-1); });
  navigator.onPressAndContinuous({B::Right}, [this] { move(1); });
}

void WrenBookmarksActivity::render(RenderLock&&) {
  using namespace wren::ui;
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  renderer.clearScreen();
  char clock[16] = {};
  halClock.formatTime(clock, sizeof(clock), false);
  drawMenuStatusBar(renderer, clock, powerManager.getBatteryPercentage());
  char count[16];
  snprintf(count, sizeof(count), "%u", static_cast<unsigned>(bookmarks.size()));
  int y = drawHeader(renderer, 28, epub ? epub->getTitle().c_str() : "", tr(STR_WREN_MENU_BOOKMARKS), count);
  if (bookmarks.empty()) {
    const char* message = tr(STR_WREN_BOOKMARKS_EMPTY);
    const char* hint = tr(STR_WREN_BOOKMARKS_EMPTY_HINT);
    const int rowHeight = renderer.getTextHeight(WREN_GOLOS_24_FONT_ID);
    const int top = BODY_TOP + std::max(0, (bodyHeight() - rowHeight - 12 - LABEL_HEIGHT) / 2);
    renderer.drawText(WREN_GOLOS_24_FONT_ID, (width - renderer.getTextWidth(WREN_GOLOS_24_FONT_ID, message)) / 2, top,
                      message);
    drawLabel(renderer, (width - labelWidth(renderer, hint)) / 2, top + rowHeight + 12, hint);
  } else {
    for (size_t i = 0; i < rows.size(); ++i) {
      const Row& row = rows[i];
      const bool selectedRow = start + static_cast<int>(i) == selected;
      if (selectedRow) renderer.fillRect(0, y, width, row.height - RULE, true);
      drawLabel(renderer, 20, y + ROW_TOP, row.meta.c_str(), !selectedRow);
      int lineY = y + ROW_TOP + LABEL_HEIGHT + EXCERPT_GAP;
      for (const auto& line : row.lines) {
        renderer.drawText(font, 20, lineY, line.c_str(), !selectedRow);
        lineY += lineHeight;
      }
      renderer.fillRect(0, y + row.height - RULE, width, RULE, true);
      y += row.height;
    }
    const char* hint = tr(STR_WREN_BOOKMARKS_HINT);
    drawLabel(renderer, (width - labelWidth(renderer, hint)) / 2, height - 54, hint);
  }
  const bool empty = bookmarks.empty();
  const Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_TAB_BACK)},
                       {empty ? TabKind::Empty : TabKind::Text, tr(STR_WREN_TAB_SELECT)},
                       {empty ? TabKind::Empty : TabKind::ArrowUp},
                       {empty ? TabKind::Empty : TabKind::ArrowDown}};
  drawButtonTabs(renderer, tabs);
  renderer.displayBuffer(first ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  first = false;
}
