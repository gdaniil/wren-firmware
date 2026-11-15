#include "WrenSetupSystemActivity.h"

#include <LibraryBuilder.h>
#include <Memory.h>
#include <WiFi.h>

#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "SetupState.h"
#include "SetupUi.h"
#include "WifiCredentialStore.h"
#include "wren/WrenVersion.h"
#include "wren/send/WrenWifiActivity.h"
#include "wren/setup/ReadingCache.h"
#include "wren/ui/WrenBusyPlate.h"
#include "wren/ui/WrenDeleteDialog.h"
#include "wren/ui/WrenFonts.h"
#include "wren/ui/WrenLayout.h"

namespace {
constexpr Language LANGUAGES[] = {Language::EN, Language::UK};
int languageChoice() {
  return wren::setup::languageIndex(static_cast<int>(I18N.getLanguage()), static_cast<int>(Language::EN),
                                    static_cast<int>(Language::UK));
}
// Lowercase the initial ASCII/Cyrillic capital without allocating a display string.
void languageName(char* out, size_t capacity) {
  const char* name = I18N.getLanguageName(I18N.getLanguage());
  snprintf(out, capacity, "%s", name);
  if (out[0] >= 'A' && out[0] <= 'Z')
    out[0] += 'a' - 'A';
  else if (static_cast<unsigned char>(out[0]) == 0xd0 && static_cast<unsigned char>(out[1]) >= 0x90 &&
           static_cast<unsigned char>(out[1]) <= 0xaf) {
    unsigned cp = 0x410 + static_cast<unsigned char>(out[1]) - 0x90 + 0x20;
    out[0] = 0xc0 | (cp >> 6);
    out[1] = 0x80 | (cp & 63);
  } else if (static_cast<unsigned char>(out[0]) == 0xd0 && static_cast<unsigned char>(out[1]) == 0x84)
    out[1] = 0xb4;
  else if (static_cast<unsigned char>(out[0]) == 0xd0 &&
           (static_cast<unsigned char>(out[1]) == 0x86 || static_cast<unsigned char>(out[1]) == 0x87)) {
    out[0] = 0xd1;
    out[1] += 0x10;
  }
}
}  // namespace
void WrenSetupSystemActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}
void WrenSetupSystemActivity::close() {
  if (changed) SETTINGS.saveToFile();
  finish();
}
bool WrenSetupSystemActivity::handleHomeGesture() {
  if (changed) SETTINGS.saveToFile();
  activityManager.goHome();
  return true;
}
void WrenSetupSystemActivity::confirmClearCache() {
  // Paint this screen first: the dialog repaints it as its backdrop on every render.
  requestUpdateAndWait();
  auto dialog = makeUniqueNoThrow<WrenDeleteDialog>(
      renderer, mappedInput, tr(STR_WREN_CACHE_TITLE), tr(STR_WREN_CACHE_BODY), tr(STR_WREN_CACHE_CLEAR),
      [](void* context) { static_cast<WrenSetupSystemActivity*>(context)->drawPage(); }, this);
  if (!dialog) {
    LOG_ERR("SETUP", "OOM: cache dialog");
    return;
  }
  startActivityForResult(std::move(dialog), [this](const ActivityResult& result) {
    if (!result.isCancelled) {
      using wren::ui::BusyPose;
      {
        RenderLock lock;
        wren::ui::drawBusyPlate(renderer, BusyPose::Wait, tr(STR_WREN_CACHE_CLEARING));
        renderer.displayBuffer();
      }
      const ReadingCacheResult cache = clearReadingCache();
      const bool ok = cache.opened && cache.failed == 0;
      {
        RenderLock lock;
        wren::ui::drawBusyPlate(renderer, ok ? BusyPose::Wait : BusyPose::Error,
                                ok ? tr(STR_WREN_CACHE_CLEARED) : tr(STR_WREN_CACHE_FAILED));
        renderer.displayBuffer();
      }
      delay(1200);
    }
    RenderLock lock;
    fullRefresh = true;
  });
}
void WrenSetupSystemActivity::change(int delta) {
  using namespace wren::setup;
  switch (static_cast<SystemRow>(selected)) {
    case SystemRow::SleepAfter: {
      const int options[] = {5, 10, 30};
      SETTINGS.sleepTimeoutMinutes = options[cycle(sleepAfterIndex(SETTINGS.sleepTimeoutMinutes), delta, 3)];
      break;
    }
    case SystemRow::Language: {
      const auto language = LANGUAGES[cycle(languageChoice(), delta, 2)];
      I18N.setLanguage(language);
      SETTINGS.language = static_cast<uint8_t>(language);
      SETTINGS.saveToFile();
      break;
    }
    case SystemRow::Titles:
      SETTINGS.libraryUseMetadata = !SETTINGS.libraryUseMetadata;
      library::markLibraryIndexDirty();
      break;
    case SystemRow::Wifi:
    case SystemRow::ClearCache:
    case SystemRow::Rebuild:
      return;
    case SystemRow::Count:
      return;
  }
  changed = true;
  requestUpdate();
}
void WrenSetupSystemActivity::loop() {
  using B = MappedInputManager::Button;
  using namespace wren::setup;
  RenderLock lock;
  const int visible = std::max(0, (renderer.getScreenHeight() - 88 - 132) / 54);
  if (wren::setup::swipeSelection(mappedInput, selected, 6, visible)) {
    requestUpdate();
    return;
  }
  int touchedRow = 0;
  const auto touch = mappedInput.rowTouch(touchedRow, 132, 54, std::min(6 - start, visible));
  if (touch != MappedInputManager::RowTouch::None) {
    selected = start + touchedRow;
    requestUpdate();
  }
  int tx = 0, ty = 0;
  const bool tapped = mappedInput.wasScreenTapped(tx, ty);
  if (mappedInput.wasReleased(B::Back) || mappedInput.wasBackGesture() || (tapped && ty >= 28 && ty < 68)) {
    lock.unlock();
    close();
    return;
  }
  if (mappedInput.wasReleased(B::Confirm) || touch == MappedInputManager::RowTouch::Tap) {
    std::unique_ptr<Activity> child;
    // Each existing child owns its resources; this parent retains only bounded UI state.
    switch (static_cast<SystemRow>(selected)) {
      case SystemRow::Wifi:
        child = makeUniqueNoThrow<WrenWifiActivity>(renderer, mappedInput, false);
        break;
      case SystemRow::ClearCache:
        lock.unlock();
        confirmClearCache();
        return;
      case SystemRow::Rebuild:
        lock.unlock();
        if (changed) SETTINGS.saveToFile();
        library::markLibraryIndexDirty();
        activityManager.goHome();
        return;
      case SystemRow::SleepAfter:
      case SystemRow::Language:
      case SystemRow::Titles:
        change(1);
        return;
      case SystemRow::Count:
        return;
    }
    if (!child) {
      LOG_ERR("SETUP", "OOM: system child activity");
      return;
    }
    const bool wifi = static_cast<SystemRow>(selected) == SystemRow::Wifi;
    lock.unlock();
    startActivityForResult(std::move(child), [this, wifi](const ActivityResult&) {
      if (wifi) {
        // This credential-only child has no session to inherit its radio.
        // Stop it in place, as the file-transfer to OPDS transition does.
        SETTINGS.saveToFile();
        if (WiFi.getMode() != WIFI_MODE_NULL) {
          WiFi.setAutoReconnect(false);
          WiFi.disconnect(false);
          if (!WiFi.mode(WIFI_OFF)) LOG_ERR("SETUP", "Could not stop Wi-Fi after network selection");
        }
      }
      RenderLock lock;
      fullRefresh = true;
    });
    return;
  }
  navigator.onPressAndContinuous({B::Left}, [this] {
    selected = (selected + 5) % 6;
    requestUpdate();
  });
  navigator.onPressAndContinuous({B::Right}, [this] {
    selected = (selected + 1) % 6;
    requestUpdate();
  });
  navigator.onPressAndContinuous({B::Up}, [this] { change(-1); });
  navigator.onPressAndContinuous({B::Down}, [this] { change(1); });
}
void WrenSetupSystemActivity::render(RenderLock&&) {
  drawPage();
  renderer.displayBuffer(fullRefresh ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  fullRefresh = false;
}
void WrenSetupSystemActivity::drawPage() {
  using namespace wren::ui;
  using namespace wren::setup;
  status(renderer);
  clipContent(renderer);
  int y = drawHeader(renderer, 28, tr(STR_WREN_TAB_SETUP), tr(STR_WREN_SETUP_SYSTEM));
  constexpr int versionHeight = 48;
  const int visible = std::max(0, (renderer.getScreenHeight() - 40 - versionHeight - y) / layout::ROW_HEIGHT);
  start = scrollStart(start, selected, visible, 6);
  const StrId labels[] = {StrId::STR_WREN_SETUP_SLEEP_AFTER, StrId::STR_WREN_SETUP_WIFI_NETWORKS,
                          StrId::STR_WREN_SETUP_LANGUAGE,    StrId::STR_WREN_SETUP_TITLES,
                          StrId::STR_WREN_SETUP_CLEAR_CACHE, StrId::STR_WREN_SETUP_REBUILD_LIBRARY};
  char timeout[16], saved[96], language[96];
  snprintf(timeout, sizeof(timeout), tr(STR_SLEEP_TIMER_VALUE_FORMAT), SETTINGS.sleepTimeoutMinutes);
  snprintf(saved, sizeof(saved), "%u %s", static_cast<unsigned>(WIFI_STORE.getCredentialCount()),
           tr(STR_WREN_SETUP_SAVED));
  languageName(language, sizeof(language));
  for (int i = start; i < 6 && i < start + visible; ++i) {
    Row row;
    row.label = I18N.get(labels[i]);
    row.selected = i == selected;
    switch (static_cast<SystemRow>(i)) {
      case SystemRow::SleepAfter:
        row.marker = Marker::Options;
        row.optionCount = 3;
        row.chosenOption = sleepAfterIndex(SETTINGS.sleepTimeoutMinutes);
        row.value = timeout;
        break;
      case SystemRow::Wifi:
        row.marker = Marker::Chevron;
        row.value = saved;
        break;
      case SystemRow::Language:
        row.marker = Marker::Options;
        row.optionCount = 2;
        row.chosenOption = languageChoice();
        row.value = language;
        row.uppercaseValue = false;
        break;
      case SystemRow::Titles:
        row.marker = Marker::Toggle;
        row.on = SETTINGS.libraryUseMetadata;
        break;
      case SystemRow::ClearCache:
      case SystemRow::Rebuild:
      case SystemRow::Count:
        break;
    }
    y = drawRow(renderer, y, row);
  }
  drawLabel(renderer, 20, y + 16, tr(STR_WREN_SETUP_VERSION));
  drawTextFit(renderer, WREN_MONO_16_FONT_ID, renderer.getScreenWidth() - 100, y + 16, WREN_VERSION, 80);
  unclip(renderer);
  footer(renderer);
}
