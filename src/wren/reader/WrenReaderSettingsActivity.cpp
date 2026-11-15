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
struct PreviewLayout {
  int boxTop = BOX_TOP;
  int boxBottom;
  int labelY = BOX_TOP + 16;
  int textTop = TEXT_TOP;
  int bottomPadding = 20;
};
PreviewLayout previewLayout(const GfxRenderer& r, int lineHeight) {
  PreviewLayout layout{BOX_TOP, r.getScreenHeight() - 54};
  if (layout.boxBottom - layout.textTop - layout.bottomPadding < lineHeight) {
    layout.boxTop = ROW_BOTTOM + 8;
    layout.labelY = layout.boxTop + 8;
    layout.textTop = layout.labelY + 16 + 4;
    layout.bottomPadding = 12;
    const int requiredBottom = layout.textTop + lineHeight + layout.bottomPadding;
    const auto insets = wren::ui::contentInsets(r);
    // Physical front hints occupy a side in landscape, leaving the lower box edge free.
    if (requiredBottom > layout.boxBottom && (insets.left || insets.right)) {
      layout.boxBottom = std::min(r.getScreenHeight() - 20, requiredBottom);
    }
  }
  return layout;
}
}  // namespace
WrenReaderSettingsActivity::WrenReaderSettingsActivity(GfxRenderer& r, MappedInputManager& i, const std::string& t,
                                                       bool live)
    : Activity("WrenReaderSettings", r, i), title(t), liveRotation(live) {}
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
  int lineHeight = std::max(1, renderer.getLineHeight(font));
  const auto layout = previewLayout(renderer, lineHeight);
  const auto insets = wren::ui::contentInsets(renderer);
  int height = std::max(0, layout.boxBottom - layout.textTop - layout.bottomPadding);
  int count = height / lineHeight;
  int width = std::max(1, renderer.getScreenWidth() - insets.left - insets.right - 76);
  // This preview wraps text without the book layout engine or hyphenation.
  lines = count ? renderer.wrappedText(font, tr(STR_WREN_SET_SAMPLE), width, count) : std::vector<std::string>{};
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
      if (liveRotation) {
        ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
        cleanRefresh = true;
      }
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
  const int lineHeight = std::max(1, renderer.getLineHeight(font));
  const auto layout = previewLayout(renderer, lineHeight);
  const auto insets = contentInsets(renderer);
  int boxHeight = layout.boxBottom - layout.boxTop;
  if (boxHeight > 0) {
    renderer.drawRect(insets.left + 20, layout.boxTop, renderer.getScreenWidth() - insets.left - insets.right - 40,
                      boxHeight, 3, true);
    drawLabel(renderer, insets.left + 38, layout.labelY, tr(STR_WREN_SET_PREVIEW));
    int lineY = layout.textTop;
    int width = renderer.getScreenWidth() - insets.left - insets.right - 76;
    for (size_t i = 0; i < lines.size(); ++i) {
      auto& line = lines[i];
      if (!justified || i + 1 == lines.size()) {
        renderer.drawText(font, insets.left + 38, lineY, line.c_str());
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
        int x = insets.left + 38, gap = 0;
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
      lineY += lineHeight;
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
