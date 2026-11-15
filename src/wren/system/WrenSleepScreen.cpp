#include "WrenSleepScreen.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "CrossPointState.h"
#include "RecentBooksStore.h"
#include "components/themes/BaseTheme.h"
#include "wren/home/BookInfo.h"
#include "wren/ui/WrenChrome.h"
#include "wren/ui/WrenFonts.h"

namespace wren::system {
namespace {
using namespace wren::ui;

void drawBattery(const GfxRenderer& r, int x, int y, int percent) {
  r.drawRect(x, y, 22, 11, 2, true);
  r.fillRect(x + 23, y + 3, 2, 5, true);
  const int fill = 16 * std::clamp(percent, 0, 100) / 100;
  if (fill) r.fillRect(x + 3, y + 3, fill, 5, true);
}

void drawCover(const GfxRenderer& r, const std::string& path, int x, int y) {
  r.drawRect(x, y, 44, 64, 2, true);
  // Home caches thumbnails at these two heights; do not create a new thumbnail on sleep.
  for (int height : {wren::home::ROW_THUMB_HEIGHT, wren::home::CARD_THUMB_HEIGHT}) {
    std::string thumb;
    if (!wren::home::thumbPath(path, height, thumb)) continue;
    HalFile file;
    if (Storage.openFileForRead("WSLEEP", thumb, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok &&
          BaseTheme::drawCoverThumbFill(r, bitmap, Rect(x + 2, y + 2, 40, 60)))
        return;
    }
  }
  r.fillRect(x + 2, y + 2, 40, 60, true);
  for (int i = 0; i < 60; i += 5) r.fillRect(x + 2, y + 2 + i, 40, 1, false);
}
}  // namespace

void drawSleepScreen(GfxRenderer& r) {
  const int width = r.getScreenWidth();
  const int height = r.getScreenHeight();
  const bool hasBook = APP_STATE.lastSleepFromReader && !APP_STATE.openEpubPath.empty();
  const int progress = hasBook ? wren::home::loadBookProgress(APP_STATE.openEpubPath) : -1;
  const int blockTop = height - 36 - (hasBook ? 98 : 40);
  r.clearScreen();

  const char* wake = tr(STR_WREN_SLEEP_WAKE_UP);
  const int arrowX = width - 46 - 18;
  const int labelH = r.getTextHeight(WREN_MONO_16_FONT_ID);
  drawLabel(r, arrowX - 8 - labelWidth(r, wake), 14 + (24 - labelH) / 2, wake);
  drawIcon(r, icons::WakeArrow, arrowX, 14, 2);
  drawIcon(r, icons::WrenSleeping, (width - 192) / 2, (blockTop - 192) / 2, 6);

  if (!hasBook) {
    const int percent = std::clamp<int>(powerManager.getBatteryPercentage(), 0, 100);
    char value[8];
    snprintf(value, sizeof(value), "%d%%", percent);
    r.fillRect(36, blockTop, width - 72, 3, true);
    const int y = height - 36 - labelH;
    drawLabel(r, 36, y, tr(STR_WREN_SLEEP_BATTERY));
    const int valueX = width - 36 - labelWidth(r, value);
    drawLabel(r, valueX, y, value);
    drawBattery(r, valueX - 8 - 25, y + (labelH - 11) / 2, percent);
    return;
  }

  const std::string& path = APP_STATE.openEpubPath;
  const RecentBook* book = nullptr;
  for (const auto& recent : RECENT_BOOKS.getBooks()) {
    if (recent.path == path) {
      book = &recent;
      break;
    }
  }
  // The fallback string owns the filename after removing its extension.
  std::string fallback = path.substr(path.find_last_of('/') == std::string::npos ? 0 : path.find_last_of('/') + 1);
  const size_t dot = fallback.find_last_of('.');
  if (dot != std::string::npos && dot != 0) fallback.erase(dot);
  const char* title = book && !book->title.empty() ? book->title.c_str() : fallback.c_str();
  const char* author = book && !book->author.empty() ? book->author.c_str() : tr(STR_WREN_LIB_UNKNOWN_AUTHOR);
  char value[8];
  snprintf(value, sizeof(value), progress < 0 ? "--%%" : "%d%%", progress);

  r.drawRect(36, blockTop, width - 72, 98, 3, true);
  const int x = 36 + 3 + 16;
  const int y = blockTop + 3 + 14;
  drawCover(r, path, x, y);
  const int textX = x + 44 + 16;
  const int valueX = width - 36 - 3 - 16 - labelWidth(r, value);
  drawTextFit(r, WREN_GOLOS_24_FONT_ID, textX, y, title, std::max(0, valueX - 16 - textX));
  drawLabel(r, valueX, y, value);
  drawTextFit(r, WREN_GOLOS_18_FONT_ID, textX, y + r.getTextHeight(WREN_GOLOS_24_FONT_ID) + 6, author,
              std::max(0, valueX - 16 - textX));
}
}  // namespace wren::system
