#include "WrenHomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>
#include <LibraryBuilder.h>
#include <LibraryFormat.h>
#include <LibraryText.h>
#include <Logging.h>
#include <Memory.h>
#include <Xtc.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "BookInfo.h"
#include "CrossPointSettings.h"
#include "HomeState.h"
#include "components/themes/BaseTheme.h"
#include "util/BookCacheUtils.h"
#include "util/BookProgress.h"
#include "wren/ui/WrenChrome.h"
#include "wren/ui/WrenDeleteDialog.h"
#include "wren/ui/WrenFonts.h"
#include "wren/ui/WrenKeyboardActivity.h"
#include "wren/ui/WrenLayout.h"

namespace {
using library::SortOrder;
constexpr auto ORDER = SortOrder::RecentDesc;
constexpr uint16_t NONE = 0xFFFF;
constexpr const char* CACHE_DIR = "/.crosspoint";

// Geometry from the canvas (X3 portrait px).
constexpr int STATUS_H = 28;
constexpr int HEADER_H = 80;
constexpr int CONTENT_TOP = STATUS_H + HEADER_H;
constexpr int GAP = 12;
constexpr int CARD_H = 232;
constexpr int SECTION_H = 40;
constexpr int ROW_H = 112;
constexpr int HINTS_H = 40;
constexpr int CARD_COVER_W = 132;
constexpr int CARD_COVER_H = wren::home::CARD_THUMB_HEIGHT + 4;
constexpr int ROW_COVER_W = 62;
constexpr int ROW_COVER_H = wren::home::ROW_THUMB_HEIGHT + 4;
constexpr int RIGHT_W = 112;
constexpr unsigned long LONG_PRESS_MS = 1000;

constexpr int LABEL = WREN_MONO_16_FONT_ID;
constexpr int G18 = WREN_GOLOS_18_FONT_ID;
constexpr int G18B = WREN_GOLOS_18_SEMIBOLD_FONT_ID;
constexpr int G24 = WREN_GOLOS_24_FONT_ID;
constexpr int G32 = WREN_GOLOS_32_FONT_ID;
constexpr int G40 = WREN_GOLOS_40_FONT_ID;
constexpr int G56 = WREN_GOLOS_56_HEADLINE_FONT_ID;

uint32_t pathHash(const std::string& path) { return wren::home::titleHash(path.c_str()); }

// Text block of a fixed line height: the top that centres the font's glyph box in it.
int lineTop(const GfxRenderer& r, int font, int top, int lineHeight) {
  return top + (lineHeight - r.getTextHeight(font)) / 2;
}

// Frame of width 2 in the content colour, inside cleared to the background, fill from the left.
void drawBar(const GfxRenderer& r, int x, int y, int w, int h, int percent, bool ink) {
  if (w < 6) return;
  r.fillRect(x, y, w, h, !ink);
  r.drawRect(x, y, w, h, 2, ink);
  percent = std::max(0, std::min(100, percent));
  if (percent > 0) r.fillRect(x + 2, y + 2, std::max(6, (w - 4) * percent / 100), h - 4, ink);
}

std::string stripExtension(const std::string& name) {
  const size_t dot = name.rfind('.');
  return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

// Chapter (1-based TOC entry) of the saved position, 0 when unknown.
int savedChapter(const std::string& path) {
  if (!FsHelpers::hasEpubExtension(path)) return 0;
  // Epub objects exceed the stack budget; one lives here for the call.
  auto epub = makeUniqueNoThrow<Epub>(path, CACHE_DIR);
  if (!epub || !epub->load(false, true)) return 0;
  HalFile file;
  uint8_t data[2]{};
  if (!Storage.openFileForRead("WHOME", epub->getCachePath() + "/progress.bin", file) || file.read(data, 2) != 2) {
    return 0;
  }
  const int toc = epub->getTocIndexForSpineIndex(data[0] | (data[1] << 8));
  return toc >= 0 ? toc + 1 : 0;
}
}  // namespace

WrenHomeActivity::WrenHomeActivity(GfxRenderer& r, MappedInputManager& input, bool clean)
    : Activity("Home", r, input), cleanInitialRefresh(clean) {}

void WrenHomeActivity::onEnter() {
  // The render task must not read the index or the recents before they are in place.
  RenderLock lock(*this);
  Activity::onEnter();
  if (RECENT_BOOKS.pruneMissing()) RECENT_BOOKS.saveToFile();
  openIndex();
  resolveRecents();
  focus = hasContinue ? -1 : 0;
  start = 0;
  // Entered while Confirm was still held (back from the reader): its release must not open a book.
  swallowConfirm = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  requestUpdate(true);
}

void WrenHomeActivity::onExit() {
  index.close();
  matches.reset();
  Activity::onExit();
}

// --- data -----------------------------------------------------------------------------------------------------

void WrenHomeActivity::openIndex() {
  const bool readMetadata = SETTINGS.libraryUseMetadata != 0;
  const bool rebuild = library::isLibraryIndexDirty() || !index.open(library::libraryIndexPath()) ||
                       index.header().metadataEnabled != readMetadata;
  if (!rebuild) return;
  index.close();
  drawScanning();
  library::BuildStats stats;
  if (!library::buildLibraryIndex("/", stats, readMetadata)) LOG_ERR("WHOME", "index build failed");
  if (!index.open(library::libraryIndexPath())) LOG_ERR("WHOME", "cannot open library index");
}

void WrenHomeActivity::resolveRecents() {
  hasContinue = false;
  continueChapter = 0;
  pinnedCount = 0;
  skippedCount = 0;
  invalidateRows();
  const auto& books = RECENT_BOOKS.getBooks();
  anyOpened = !books.empty();
  const int total = index.bookCount();
  const int count = std::min<int>(books.size(), RecentBooksStore::MAX_RECENT_BOOKS);
  if (count == 0 || total == 0) return;

  library::BookIdentity ids[RecentBooksStore::MAX_RECENT_BOOKS];
  uint16_t asc[RecentBooksStore::MAX_RECENT_BOOKS];
  for (int i = 0; i < count; ++i) {
    const std::string& path = books[i].path;
    ids[i].pathHash = library::clixPathHash(path.data(), path.size());
    ids[i].fileSize = 0;  // 0 = match by path hash only
    HalFile file;
    if (Storage.openFileForRead("WHOME", path.c_str(), file)) ids[i].fileSize = static_cast<uint32_t>(file.fileSize());
    asc[i] = NONE;
  }
  if (!index.recentRowsFor(ids, count, asc)) {
    LOG_ERR("WHOME", "recent-book lookup failed");
    return;
  }
  for (int i = 0; i < count; ++i) {
    if (asc[i] == NONE || asc[i] >= total) continue;
    const uint16_t row = static_cast<uint16_t>(total - 1 - asc[i]);
    if (std::find(skippedRows, skippedRows + skippedCount, row) != skippedRows + skippedCount) continue;
    skippedRows[skippedCount++] = row;
    if (i == 0) {
      hasContinue = loadBook(index.ordinalForRow(ORDER, row), continueBook);
    } else {
      pinnedRows[pinnedCount++] = row;
    }
  }
  std::sort(skippedRows, skippedRows + skippedCount);
  if (hasContinue) continueChapter = savedChapter(continueBook.path);
}

int WrenHomeActivity::listCount() const {
  if (!query.empty()) return matchCount;
  const int total = index.bookCount();
  return std::max(0, total - skippedCount + pinnedCount);
}

uint16_t WrenHomeActivity::ordinalForListRow(int row) {
  if (row < 0 || row >= listCount()) return NONE;
  if (!query.empty()) return index.ordinalForRow(ORDER, matches[row]);
  if (row < pinnedCount) return index.ordinalForRow(ORDER, pinnedRows[row]);
  const uint16_t sortRow =
      wren::home::sortRowFor(static_cast<uint16_t>(row - pinnedCount), skippedRows, skippedCount, index.bookCount());
  return sortRow == wren::home::NO_BOOK ? NONE : index.ordinalForRow(ORDER, sortRow);
}

bool WrenHomeActivity::loadBook(uint16_t ordinal, Book& out) {
  out = Book{};
  library::ClixRecord record{};
  if (ordinal == NONE || !index.readRecord(ordinal, record) || !index.readPath(record, out.path)) return false;
  if (!index.readTitle(record, out.title) || out.title.empty()) {
    std::string name;
    if (index.readName(record, name)) out.title = stripExtension(name);
  }
  index.readAuthor(record, out.author);
  out.progress = wren::home::loadBookProgress(out.path);
  out.ordinal = ordinal;
  return true;
}

void WrenHomeActivity::invalidateRows() {
  for (auto& book : rows) book = Book{};
  rowsFirst = -1;
}

WrenHomeActivity::Book* WrenHomeActivity::rowBook(int row) {
  if (rowsFirst != start) {
    const int shift = start - rowsFirst;
    if (rowsFirst < 0 || shift >= MAX_ROWS || shift <= -MAX_ROWS) {
      invalidateRows();
    } else if (shift > 0) {
      // Keep the books still on screen; their progress and covers are already read.
      std::rotate(rows, rows + shift, rows + MAX_ROWS);
      for (int i = MAX_ROWS - shift; i < MAX_ROWS; ++i) rows[i] = Book{};
    } else {
      std::rotate(rows, rows + MAX_ROWS + shift, rows + MAX_ROWS);
      for (int i = 0; i < -shift; ++i) rows[i] = Book{};
    }
    rowsFirst = start;
  }
  const int slot = row - start;
  if (slot < 0 || slot >= MAX_ROWS) return nullptr;
  if (rows[slot].ordinal == NONE && !loadBook(ordinalForListRow(row), rows[slot])) return nullptr;
  return &rows[slot];
}

void WrenHomeActivity::applySearch() {
  matches.reset();
  matchCount = 0;
  if (query.empty()) return;
  const std::string needle = library::fold(query);
  const int total = index.bookCount();
  if (total <= 0) return;
  // One sort row per indexed book at most: 2 bytes each, fallible.
  auto found = makeUniqueNoThrow<uint16_t[]>(static_cast<size_t>(total));
  if (!found) {
    LOG_ERR("WHOME", "OOM: search results");
    return;
  }
  std::string author;
  for (int row = 0; row < total; ++row) {
    const uint16_t ordinal = index.ordinalForRow(ORDER, static_cast<uint16_t>(row));
    library::ClixRecord record{};
    if (ordinal == NONE || !index.readRecord(ordinal, record)) continue;
    author.clear();
    if (library::matchesQuery(std::string_view(record.fold, record.foldLen), needle) ||
        (index.readAuthor(record, author) && library::matchesQuery(library::fold(author), needle))) {
      found[matchCount++] = static_cast<uint16_t>(row);
    }
  }
  matches = std::move(found);
}

WrenHomeActivity::Cover WrenHomeActivity::coverState(Book& book, int height) {
  if (book.cover != Cover::Unknown) return book.cover;
  if (!wren::home::thumbPath(book.path, height, book.thumb)) {
    book.cover = Cover::Missing;
  } else if (Storage.exists(book.thumb.c_str())) {
    book.cover = Cover::Ready;
  } else {
    book.cover = failed(book.path) ? Cover::Missing : Cover::Pending;
  }
  return book.cover;
}

bool WrenHomeActivity::generateCover(Book& book, int height) {
  bool ok = false;
  if (FsHelpers::hasReflowableBookExtension(book.path)) {
    auto epub = makeUniqueNoThrow<Epub>(book.path, CACHE_DIR);
    ok = epub && epub->generateThumbBmpFromSource(height);
  } else if (FsHelpers::hasXtcExtension(book.path)) {
    auto xtc = makeUniqueNoThrow<Xtc>(book.path, CACHE_DIR);
    ok = xtc && xtc->load() && xtc->generateThumbBmp(height);
  }
  ok = ok && Storage.exists(book.thumb.c_str());
  if (!ok) markFailed(book.path);
  book.cover = ok ? Cover::Ready : Cover::Missing;
  return true;
}

// One cover per loop pass, in reading order of the screen, so the list stays responsive.
bool WrenHomeActivity::generateOneCover() {
  if (index.bookCount() == 0) return false;
  if (cardVisible() && continueBook.cover == Cover::Pending) return generateCover(continueBook, CARD_COVER_H - 4);
  const int count = listCount();
  const int visible = visibleRows();
  for (int i = 0; i < visible && start + i < count; ++i) {
    const int slot = start + i - rowsFirst;
    if (rowsFirst < 0 || slot < 0 || slot >= MAX_ROWS) continue;
    if (rows[slot].ordinal != NONE && rows[slot].cover == Cover::Pending) {
      return generateCover(rows[slot], ROW_COVER_H - 4);
    }
  }
  return false;
}

bool WrenHomeActivity::failed(const std::string& path) const {
  const uint32_t h = pathHash(path);
  return std::find(failedCovers, failedCovers + failedCount, h) != failedCovers + failedCount;
}

void WrenHomeActivity::markFailed(const std::string& path) {
  if (failed(path)) return;
  // Oldest entry goes when the list is full; that cover is tried once more later.
  if (failedCount == MAX_FAILED) {
    std::rotate(failedCovers, failedCovers + 1, failedCovers + MAX_FAILED);
    --failedCount;
  }
  failedCovers[failedCount++] = pathHash(path);
}

// --- layout ---------------------------------------------------------------------------------------------------

int WrenHomeActivity::contentTop() const { return CONTENT_TOP; }

bool WrenHomeActivity::cardVisible() const {
  if (!hasContinue || !query.empty()) return false;
  if (focus < 0) return true;
  // Landscape: no row fits under the card, so the card steps aside while a row has focus.
  const int rowsTopWithCard = CONTENT_TOP + GAP + CARD_H + GAP + SECTION_H;
  return (renderer.getScreenHeight() - HINTS_H - rowsTopWithCard) / ROW_H >= 1;
}

int WrenHomeActivity::rowsTop() const {
  int y = CONTENT_TOP;
  if (cardVisible()) y += GAP + CARD_H;
  return y + GAP + SECTION_H;
}

int WrenHomeActivity::visibleRows() const {
  return std::max(0, std::min(MAX_ROWS, (renderer.getScreenHeight() - HINTS_H - rowsTop()) / ROW_H));
}

// --- drawing --------------------------------------------------------------------------------------------------

void WrenHomeActivity::drawHeader() const { wren::ui::drawTabHeader(renderer, STATUS_H, 0); }

void WrenHomeActivity::drawCover(int x, int y, int w, int h, Book& book, bool ink, bool plate) {
  using namespace wren::ui;
  renderer.drawRect(x, y, w, h, 2, ink);
  const int ix = x + 2, iy = y + 2, iw = w - 4, ih = h - 4;
  renderer.fillRect(ix, iy, iw, ih, false);
  if (coverState(book, ih) == Cover::Ready) {
    HalFile file;
    if (Storage.openFileForRead("WHOME", book.thumb, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok &&
          BaseTheme::drawCoverThumbFill(renderer, bitmap, Rect(ix, iy, iw, ih))) {
        return;
      }
    }
    book.cover = Cover::Missing;
  }
  // No cover: a pattern chosen from the title, ink on paper whatever the row colour.
  switch (wren::home::coverPattern(book.title.c_str())) {
    case 0:
      renderer.fillRect(ix, iy, iw, ih, true);
      break;
    case 1:
      renderer.fillRectDither(ix, iy, iw, ih, Color::DarkGray);
      break;
    case 2:
      for (int j = 0; j < ih; ++j)
        for (int i = (4 - j % 4) % 4; i < iw; i += 4) renderer.drawPixel(ix + i, iy + j, true);
      break;
    case 3:
      for (int j = 1; j < ih; j += 3)
        for (int i = 1; i < iw; i += 3) renderer.drawPixel(ix + i, iy + j, true);
      break;
    default:
      for (int j = 0; j < ih; j += 5) renderer.fillRect(ix, iy + j, iw, 1, true);
      break;
  }
  if (!plate) return;
  const int pw = iw - 16;
  const int lines = std::max(1, wrappedLineCount(renderer, G18B, book.title.c_str(), pw - 10, 3));
  const int ph = lines * 19 + 8;
  const int px = ix + 8, py = iy + ih - 8 - ph;
  renderer.fillRect(px, py, pw, ph, false);
  drawTextWrapped(renderer, G18B, px + 5, py + 4, book.title.c_str(), pw - 10, 3, 19, true);
}

void WrenHomeActivity::drawCard(int y, bool focused) {
  using namespace wren::ui;
  const bool ink = !focused;
  const int x = 20, w = renderer.getScreenWidth() - 40;
  if (focused) {
    renderer.fillRect(x, y, w, CARD_H, true);
  } else {
    renderer.drawRect(x, y, w, CARD_H, 3, true);
  }
  drawCover(x + 16, y + 16, CARD_COVER_W, CARD_COVER_H, continueBook, ink, true);
  const int cx = x + 16 + CARD_COVER_W + 20;
  const int cw = x + w - 16 - cx;
  char label[64];
  if (continueChapter > 0) {
    char chapter[24];
    snprintf(chapter, sizeof(chapter), tr(STR_WREN_LIB_CHAPTER), continueChapter);
    snprintf(label, sizeof(label), "%s · %s", tr(STR_WREN_LIB_CONTINUE), chapter);
  } else {
    snprintf(label, sizeof(label), "%s", tr(STR_WREN_LIB_CONTINUE));
  }
  drawLabel(renderer, cx, y + 16, label, ink);
  const int titleTop = y + 16 + 19 + 14;
  const int lines = drawTextWrapped(renderer, G40, cx, titleTop, continueBook.title.c_str(), cw, 2, 40, ink, -2);
  const char* author = continueBook.author.empty() ? tr(STR_WREN_LIB_UNKNOWN_AUTHOR) : continueBook.author.c_str();
  drawTextFit(renderer, G18, cx, titleTop + std::max(1, lines) * 40 + 10, author, cw, ink);
  const int percent = std::max(0, continueBook.progress);
  const int labelY = y + CARD_H - 16 - 19;
  drawBar(renderer, cx, labelY - 10 - 10, cw, 10, percent, ink);
  char number[8];
  snprintf(number, sizeof(number), "%d%%", percent);
  drawLabel(renderer, cx, labelY, number, ink);
}

void WrenHomeActivity::drawSection(int y) {
  using namespace wren::ui;
  const int sw = renderer.getScreenWidth();
  const int ty = lineTop(renderer, LABEL, y, SECTION_H - 3);
  char count[12];
  snprintf(count, sizeof(count), "%d", query.empty() ? static_cast<int>(index.bookCount()) : listCount());
  const int countX = sw - 20 - labelWidth(renderer, count);
  drawLabel(renderer, countX, ty, count);
  if (query.empty()) {
    drawLabel(renderer, 20, ty, tr(STR_WREN_LIB_ALL_BOOKS));
  } else {
    // The query is the reader's own text: plain font, cut to the space left.
    drawTextFit(renderer, G18, 20, lineTop(renderer, G18, y, SECTION_H - 3), query.c_str(), countX - 16 - 20);
  }
  renderer.fillRect(0, y + SECTION_H - 3, sw, 3, true);
}

void WrenHomeActivity::drawRow(int y, Book& book, bool focused) {
  using namespace wren::ui;
  const int sw = renderer.getScreenWidth();
  const bool ink = !focused;
  const int h = ROW_H - 2;
  if (focused) renderer.fillRect(0, y, sw, h, true);
  drawCover(20, y + (h - ROW_COVER_H) / 2, ROW_COVER_W, ROW_COVER_H, book, ink, false);
  const int mx = 20 + ROW_COVER_W + 16;
  const int rx = sw - 20 - RIGHT_W;
  const int mw = rx - 16 - mx;
  const int textTop = y + (h - (30 + 2 + 22)) / 2;
  drawTextFit(renderer, G24, mx, lineTop(renderer, G24, textTop, 30), book.title.c_str(), mw, ink, -1);
  const char* author = book.author.empty() ? tr(STR_WREN_LIB_UNKNOWN_AUTHOR) : book.author.c_str();
  drawTextFit(renderer, G18, mx, lineTop(renderer, G18, textTop + 32, 22), author, mw, ink);

  if (book.progress < 0) {
    const char* label = tr(STR_WREN_LIB_NEW);
    drawLabel(renderer, sw - 20 - labelWidth(renderer, label), y + (h - 19) / 2, label, ink);
  } else if (book.progress >= 100) {
    const int top = y + (h - (14 + 8 + 8 + 19)) / 2;
    const int barY = top + 14;
    renderer.fillRect(rx, barY, RIGHT_W, 8, ink);
    drawIcon(renderer, icons::WrenDone, rx + RIGHT_W - 2 - 32, barY - 6 - 32, 1, ink);
    drawLabel(renderer, rx, barY + 16, tr(STR_WREN_LIB_DONE), ink);
  } else {
    const int barY = y + (h - (8 + 8 + 19)) / 2;
    drawBar(renderer, rx, barY, RIGHT_W, 8, book.progress, ink);
    char number[8];
    snprintf(number, sizeof(number), "%d%%", book.progress);
    drawLabel(renderer, rx, barY + 16, number, ink);
  }
  renderer.fillRect(0, y + h, sw, 2, true);
}

void WrenHomeActivity::drawEmpty(int top) {
  using namespace wren::ui;
  const int sw = renderer.getScreenWidth();
  const int bottom = renderer.getScreenHeight() - HINTS_H;
  const char* body = tr(STR_WREN_LIB_EMPTY_BODY);
  const int bodyLines = std::max(1, wrappedLineCount(renderer, G18, body, sw - 40, 4));
  const int textBlock = 24 + 56 + 14 + bodyLines * 25 + 28 + 72;
  // Landscape has no room for the 128 px drawing and the bottom air: halve the one, drop the other.
  int scale = 4;
  int padBottom = 120;
  if (bottom - top < 32 * scale + textBlock + padBottom) padBottom = 0;
  if (bottom - top < 32 * scale + textBlock) scale = 2;
  const int total = 32 * scale + textBlock;
  int y = top + std::max(0, (bottom - padBottom - top - total) / 2);
  drawIcon(renderer, icons::EmptyShelf, 20, y, scale, true);
  y += 32 * scale + 24;
  drawTextFit(renderer, G56, 20, lineTop(renderer, G56, y, 56), tr(STR_WREN_LIB_EMPTY_TITLE), sw - 40, true, -2);
  y += 56 + 14;
  drawTextWrapped(renderer, G18, 20, y, body, sw - 40, 4, 25, true);
  y += bodyLines * 25 + 28;
  renderer.fillRect(20, y, sw - 40, 72, true);
  drawTextFit(renderer, G32, 40, lineTop(renderer, G32, y, 72), tr(STR_WREN_LIB_SEND_BOOKS), sw - 40 - 40 - 48, false,
              -1);
  drawIcon(renderer, icons::ChevronBig, sw - 20 - 20 - 32, y + 20, 1, false);
}

void WrenHomeActivity::drawHelp(int y) {
  using namespace wren::ui;
  if (renderer.getScreenHeight() - HINTS_H - y < 36 + 64 + 16 + 40) return;
  drawIcon(renderer, icons::WrenGlasses, 20, y + 36, 2, true);
  drawTextFit(renderer, G40, 20, lineTop(renderer, G40, y + 36 + 64 + 16, 40), tr(STR_WREN_LIB_PICK),
              renderer.getScreenWidth() - 40, true, -2);
}

void WrenHomeActivity::drawHints() {
  using namespace wren::ui;
  const bool empty = index.bookCount() == 0;
  Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_TAB_SEARCH)},
                 {TabKind::Text, tr(STR_WREN_TAB_SELECT)},
                 {TabKind::ArrowUp},
                 {TabKind::ArrowDown}};
  if (menuOpen) {
    tabs[0].text = tr(STR_WREN_TAB_CLOSE);
  } else if (empty) {
    tabs[0] = {TabKind::Empty};
  } else if (!query.empty()) {
    tabs[0].text = tr(STR_WREN_TAB_BACK);
  }
  drawButtonTabs(renderer, tabs);
  const bool bookFocused = !menuOpen && !empty && (focus < 0 ? cardVisible() : focus < listCount());
  if (!bookFocused || gpio.hasTouch()) return;
  // "Hold: menu" plate sitting on the select hint. Hints are drawn in the panel's portrait frame.
  const auto old = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Portrait);
  const char* label = tr(STR_WREN_TAB_MENU);
  const int pw = labelWidth(renderer, label) + 16;
  const int px = layout::tabX(renderer.getScreenWidth(), 1) + (106 - pw) / 2;
  const int py = renderer.getScreenHeight() - 28 - 24;
  renderer.fillRect(px, py, pw, 24, true);
  drawLabel(renderer, px + 8, lineTop(renderer, LABEL, py, 24), label, false);
  renderer.setOrientation(old);
}

void WrenHomeActivity::drawMenu() {
  using namespace wren::ui;
  const Book* book =
      focus < 0 ? &continueBook : (focus - start >= 0 && focus - start < MAX_ROWS ? &rows[focus - start] : nullptr);
  if (!book) return;
  const int sw = renderer.getScreenWidth();
  renderer.fillRectDither(0, 0, sw, renderer.getScreenHeight() - HINTS_H, Color::DarkGray,
                          GfxRenderer::DitherMode::PreserveInk);
  const int x = 32, w = sw - 64, top = 170;
  const int head = 16 + 30 + 2 + 22 + 16;
  const int h = 3 + head + 3 + 64 + 2 + 64 + 3;
  renderer.fillRect(x, top, w, h, false);
  renderer.drawRect(x, top, w, h, 3, true);
  drawTextFit(renderer, G24, x + 24, lineTop(renderer, G24, top + 3 + 16, 30), book->title.c_str(), w - 48, true, -1);
  const char* author = book->author.empty() ? tr(STR_WREN_LIB_UNKNOWN_AUTHOR) : book->author.c_str();
  drawTextFit(renderer, G18, x + 24, lineTop(renderer, G18, top + 3 + 16 + 32, 22), author, w - 48);
  int y = top + 3 + head;
  renderer.fillRect(x, y, w, 3, true);
  y += 3;
  const char* items[] = {tr(STR_WREN_LIB_OPEN), tr(STR_WREN_LIB_DELETE)};
  for (int i = 0; i < 2; ++i) {
    const bool focused = menuFocus == i;
    if (focused) renderer.fillRect(x + 3, y, w - 6, 64, true);
    drawTextFit(renderer, G24, x + 24, lineTop(renderer, G24, y, 64), items[i], w - 48 - 40, !focused, -1);
    if (i == 1) drawIcon(renderer, icons::Trash, x + w - 24 - 32, y + 16, 1, !focused);
    y += 64;
    if (i == 0) {
      renderer.fillRect(x, y, w, 2, true);
      y += 2;
    }
  }
}

void WrenHomeActivity::drawScanning() {
  using namespace wren::ui;
  renderer.clearScreen();
  char clock[16] = {};
  halClock.formatTime(clock, sizeof(clock), false);
  drawMenuStatusBar(renderer, clock, powerManager.getBatteryPercentage());
  drawHeader();
  const int sw = renderer.getScreenWidth();
  const int y = CONTENT_TOP + GAP;
  const int boxH = 20 + 64 + 20;
  renderer.drawRect(20, y, sw - 40, boxH, 3, true);
  drawIcon(renderer, icons::WrenScanning, 40, y + 20, 2, true);
  const int tx = 40 + 64 + 20;
  const int textTop = y + 20 + (64 - (19 + 8 + 40)) / 2;
  drawLabel(renderer, tx, textTop, tr(STR_WREN_LIB_SCANNING));
  drawTextFit(renderer, G40, tx, lineTop(renderer, G40, textTop + 19 + 8, 40), tr(STR_WREN_LIB_LOOKING), sw - 40 - tx,
              true, -2);
  drawLabel(renderer, 20, y + boxH + GAP, tr(STR_WREN_LIB_KEEP_CARD));
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void WrenHomeActivity::drawPage() {
  using namespace wren::ui;
  renderer.clearScreen();
  char clock[16] = {};
  halClock.formatTime(clock, sizeof(clock), false);
  drawMenuStatusBar(renderer, clock, powerManager.getBatteryPercentage());
  drawHeader();
  if (index.bookCount() == 0) {
    drawEmpty(CONTENT_TOP);
  } else {
    int y = CONTENT_TOP;
    if (cardVisible()) {
      drawCard(y + GAP, focus < 0);
      y += GAP + CARD_H;
    }
    y += GAP;
    drawSection(y);
    y += SECTION_H;
    const int count = listCount();
    const int visible = visibleRows();
    start = wren::home::scrollStart(start, focus, visible, count);
    for (int i = 0; i < visible && start + i < count; ++i) {
      Book* book = rowBook(start + i);
      if (book) drawRow(y, *book, focus == start + i);
      y += ROW_H;
    }
    if (!anyOpened && query.empty()) drawHelp(y);
  }
  drawHints();
  drawEdgeTabs(renderer, true, true);
  if (menuOpen) drawMenu();
}
void WrenHomeActivity::render(RenderLock&&) {
  drawPage();
  renderer.displayBuffer(firstPaint && cleanInitialRefresh ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstPaint = false;
  painted = true;
}

// --- input ----------------------------------------------------------------------------------------------------

void WrenHomeActivity::move(int delta) {
  const int count = listCount();
  const int first = hasContinue && query.empty() ? -1 : 0;
  const int next = std::max(first, std::min(std::max(first, count - 1), focus + delta));
  if (next == focus) return;
  focus = next;
  start = wren::home::scrollStart(start, focus, visibleRows(), count);
  requestUpdate();
}

void WrenHomeActivity::openFocused() {
  std::string path;
  if (focus < 0) {
    if (hasContinue) path = continueBook.path;
  } else if (Book* book = rowBook(focus)) {
    path = book->path;
  }
  if (!path.empty()) activityManager.goToReader(path);
}

void WrenHomeActivity::openSearch() {
  auto keyboard =
      makeUniqueNoThrow<WrenKeyboardActivity>(renderer, mappedInput, tr(STR_WREN_TAB_LIBRARY), tr(STR_WREN_TAB_SEARCH),
                                              tr(STR_WREN_TAB_SEARCH), query, 48, InputType::Text);
  if (!keyboard) {
    LOG_ERR("WHOME", "OOM: search keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) return;
    RenderLock lock(*this);
    query = std::get<KeyboardResult>(result.data).text;
    applySearch();
    focus = hasContinue && query.empty() ? -1 : 0;
    start = 0;
    invalidateRows();
  });
}

void WrenHomeActivity::menuAction() {
  std::string path, title;
  if (focus < 0) {
    path = continueBook.path;
    title = continueBook.title;
  } else if (Book* book = rowBook(focus)) {
    path = book->path;
    title = book->title;
  }
  menuOpen = false;
  if (path.empty()) {
    requestUpdate();
  } else if (menuFocus == 0) {
    activityManager.goToReader(path);
  } else {
    deleteBook(path, title);
  }
}

void WrenHomeActivity::deleteBook(const std::string& path, const std::string& title) {
  // Paint the library without its action menu before the child overlays it.
  requestUpdateAndWait();
  index.close();
  auto confirmation = makeUniqueNoThrow<WrenDeleteDialog>(
      renderer, mappedInput, path, title,
      [](void* context) {
        auto* home = static_cast<WrenHomeActivity*>(context);
        // Release the index again before the dialog removes files or dirties it.
        if (!home->index.open(library::libraryIndexPath())) LOG_ERR("WHOME", "cannot reopen library index");
        home->drawPage();
        home->index.close();
      },
      this);
  if (!confirmation) {
    LOG_ERR("WHOME", "OOM: delete confirmation");
    if (!index.open(library::libraryIndexPath())) LOG_ERR("WHOME", "cannot reopen library index");
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(confirmation), [this](const ActivityResult& result) {
    RenderLock lock(*this);
    if (!result.isCancelled) {
      library::BuildStats stats;
      if (!library::buildLibraryIndex("/", stats, SETTINGS.libraryUseMetadata != 0)) {
        LOG_ERR("WHOME", "index build failed");
      }
    }
    if (!index.open(library::libraryIndexPath())) LOG_ERR("WHOME", "cannot reopen library index");
    resolveRecents();
    applySearch();
    const int count = listCount();
    const int first = hasContinue && query.empty() ? -1 : 0;
    focus = std::max(first, std::min(focus, count - 1));
    start = 0;
  });
}

void WrenHomeActivity::loop() {
  using B = MappedInputManager::Button;
  RenderLock lock(*this);

  if (menuOpen) {
    if (mappedInput.wasReleased(B::Back)) {
      menuOpen = false;
      requestUpdate();
    } else if (mappedInput.wasReleased(B::Confirm)) {
      lock.unlock();
      menuAction();
    } else if (mappedInput.wasPressed(B::Left) || mappedInput.wasPressed(B::Right)) {
      menuFocus = mappedInput.wasPressed(B::Left) ? 0 : 1;
      requestUpdate();
    }
    return;
  }

  if (swallowConfirm && mappedInput.wasReleased(B::Confirm)) {
    swallowConfirm = false;
    return;
  }
  const bool empty = index.bookCount() == 0;
  if (mappedInput.wasPressed(B::Down)) {
    lock.unlock();
    activityManager.goToFileTransfer();  // next tab: send
    return;
  }
  if (mappedInput.wasPressed(B::Up)) {
    lock.unlock();
    activityManager.goToSettings();  // previous tab, wrapping: setup
    return;
  }
  if (empty) {
    if (mappedInput.wasReleased(B::Confirm)) {
      lock.unlock();
      activityManager.goToFileTransfer();
    }
    return;
  }
  const bool bookFocused = focus < 0 ? hasContinue : focus < listCount();
  if (bookFocused && mappedInput.wasLongPressed(B::Confirm, LONG_PRESS_MS)) {
    menuOpen = true;
    menuFocus = 0;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(B::Confirm)) {
    lock.unlock();
    openFocused();
    return;
  }
  if (mappedInput.wasReleased(B::Back)) {
    if (!query.empty()) {
      query.clear();
      applySearch();
      focus = hasContinue ? -1 : 0;
      start = 0;
      invalidateRows();
      requestUpdate();
    } else {
      lock.unlock();
      openSearch();
    }
    return;
  }
  navigator.onPressAndContinuous({B::Left}, [this] { move(-1); });
  navigator.onPressAndContinuous({B::Right}, [this] { move(1); });

  // Covers missing on screen are made one at a time, after the screen without them is up.
  if (painted && generateOneCover()) {
    painted = false;
    requestUpdate();
  }
}
