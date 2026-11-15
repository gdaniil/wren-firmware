#include "WrenReaderSettingsActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <algorithm>
#include <cstring>

#include "CrossPointSettings.h"
#include "ReaderMenuState.h"
#include "SdCardFontSystem.h"
#include "activities/reader/ReaderUtils.h"
#include "wren/ui/WrenChrome.h"

namespace {
constexpr int ROW_BOTTOM = 132 + 4 * 54;
constexpr int BOX_TOP = ROW_BOTTOM + 24;
constexpr int TEXT_TOP = BOX_TOP + 16 + 16 + 12;
}  // namespace
WrenReaderSettingsActivity::WrenReaderSettingsActivity(GfxRenderer& r, MappedInputManager& i, const std::string& t)
    : Activity("WrenReaderSettings", r, i), title(t) {}
void WrenReaderSettingsActivity::onEnter() {
  Activity::onEnter();
  updatePreview();
  requestUpdate();
}
void WrenReaderSettingsActivity::close() {
  if (changed) SETTINGS.saveToFile();
  finish();
}
bool WrenReaderSettingsActivity::handleHomeGesture() {
  close();
  return true;
}
void WrenReaderSettingsActivity::updatePreview() {
  sdFontSystem.ensureLoaded(renderer);
  font = SETTINGS.getReaderFontId();
  int height = std::max(0, renderer.getScreenHeight() - 54 - 20 - TEXT_TOP);
  int lineHeight = std::max(1, renderer.getLineHeight(font));
  int count = height / lineHeight;
  // This preview wraps text without the book layout engine or hyphenation.
  lines = count
              ? renderer.wrappedText(font, tr(STR_WREN_SET_SAMPLE), std::max(1, renderer.getScreenWidth() - 76), count)
              : std::vector<std::string>{};
}
void WrenReaderSettingsActivity::change(int delta) {
  RenderLock lock;
  using namespace wren::reader;
  switch (static_cast<SettingsRow>(selected)) {
    case SettingsRow::Font:
      SETTINGS.wrenFont[SETTINGS.wrenMode] = cycleSetting(SETTINGS.wrenFont[SETTINGS.wrenMode], delta, 3);
      break;
    case SettingsRow::Size:
      SETTINGS.wrenMode = cycleSetting(SETTINGS.wrenMode, delta, 3);
      break;
    case SettingsRow::Alignment: {
      bool justified =
          SETTINGS.readerRenderSpec(renderer.getScreenWidth(), renderer.getScreenHeight()).paragraphAlignment ==
          CrossPointSettings::JUSTIFIED;
      SETTINGS.setParagraphAlignment(justified ? CrossPointSettings::LEFT_ALIGN : CrossPointSettings::JUSTIFIED);
      break;
    }
    case SettingsRow::Orientation:
      SETTINGS.orientation = toggleOrientation(SETTINGS.orientation);
      ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
      cleanRefresh = true;
      break;
  }
  changed = true;
  updatePreview();
  requestUpdate();
}
void WrenReaderSettingsActivity::loop() {
  using B = MappedInputManager::Button;
  if (mappedInput.wasReleased(B::Back)) {
    close();
    return;
  }
  if (mappedInput.wasReleased(B::Confirm)) {
    change(1);
    return;
  }
  navigator.onPressAndContinuous({B::Left}, [this] {
    selected = wren::reader::cycleSetting(selected, -1, 4);
    requestUpdate();
  });
  navigator.onPressAndContinuous({B::Right}, [this] {
    selected = wren::reader::cycleSetting(selected, 1, 4);
    requestUpdate();
  });
  navigator.onPressAndContinuous({B::PageBack}, [this] { change(-1); });
  navigator.onPressAndContinuous({B::PageForward}, [this] { change(1); });
}
void WrenReaderSettingsActivity::render(RenderLock&&) {
  using namespace wren::ui;
  renderer.clearScreen();
  char clock[16] = {};
  halClock.formatTime(clock, sizeof(clock), false);
  drawMenuStatusBar(renderer, clock, powerManager.getBatteryPercentage());
  int y = drawHeader(renderer, 28, title.c_str(), tr(STR_WREN_SETTINGS_TITLE));
  const char* fonts[] = {"literata", "golos", "bitter"};
  const char* sizes[] = {tr(STR_WREN_SET_SMALL), tr(STR_WREN_SET_STANDARD), tr(STR_WREN_SET_LARGE)};
  bool justified =
      SETTINGS.readerRenderSpec(renderer.getScreenWidth(), renderer.getScreenHeight()).paragraphAlignment ==
      CrossPointSettings::JUSTIFIED;
  bool land = wren::reader::landscape(SETTINGS.orientation);
  const char* labels[] = {tr(STR_WREN_SET_FONT), tr(STR_WREN_SET_SIZE), tr(STR_WREN_SET_ALIGN),
                          tr(STR_WREN_MENU_ORIENTATION)};
  const char* values[] = {fonts[SETTINGS.wrenFont[SETTINGS.wrenMode]], sizes[SETTINGS.wrenMode],
                          justified ? tr(STR_WREN_SET_JUSTIFIED) : tr(STR_WREN_SET_LEFT),
                          land ? tr(STR_WREN_LANDSCAPE) : tr(STR_WREN_PORTRAIT)};
  int chosen[] = {SETTINGS.wrenFont[SETTINGS.wrenMode], SETTINGS.wrenMode, justified ? 1 : 0, land ? 1 : 0};
  for (int i = 0; i < 4; ++i) {
    Row row;
    row.label = labels[i];
    row.value = values[i];
    row.marker = Marker::Options;
    row.optionCount = i < 2 ? 3 : 2;
    row.chosenOption = chosen[i];
    row.selected = selected == i;
    y = drawRow(renderer, y, row);
  }
  int boxHeight = renderer.getScreenHeight() - 54 - BOX_TOP;
  if (boxHeight > 0) {
    renderer.drawRect(20, BOX_TOP, renderer.getScreenWidth() - 40, boxHeight, 3, true);
    drawLabel(renderer, 38, BOX_TOP + 16, tr(STR_WREN_SET_PREVIEW));
    int lineY = TEXT_TOP;
    int width = renderer.getScreenWidth() - 76;
    for (size_t i = 0; i < lines.size(); ++i) {
      auto& line = lines[i];
      if (!justified || i + 1 == lines.size()) {
        renderer.drawText(font, 38, lineY, line.c_str());
      } else {
        int gaps = std::count(line.begin(), line.end(), ' ');
        int measured = gaps * renderer.getSpaceWidth(font);
        char* word = &line[0];
        char* part = word;
        while (char* space = std::strchr(part, ' ')) {
          *space = '\0';
          measured += renderer.getTextWidth(font, part);
          *space = ' ';
          part = space + 1;
        }
        measured += renderer.getTextWidth(font, part);
        int extra = width - measured;
        int x = 38, gap = 0;
        while (char* space = std::strchr(word, ' ')) {
          *space = '\0';
          renderer.drawText(font, x, lineY, word);
          x += renderer.getTextWidth(font, word) + renderer.getSpaceWidth(font) +
               wren::reader::extraSpace(extra, gaps, gap++);
          *space = ' ';
          word = space + 1;
        }
        renderer.drawText(font, x, lineY, word);
      }
      lineY += std::max(1, renderer.getLineHeight(font));
    }
  }
  const Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_TAB_BACK)},
                       {TabKind::Text, tr(STR_WREN_TAB_SELECT)},
                       {TabKind::ArrowUp},
                       {TabKind::ArrowDown}};
  drawButtonTabs(renderer, tabs);
  drawEdgeTabs(renderer, true, true);
  renderer.displayBuffer(cleanRefresh ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanRefresh = false;
}
