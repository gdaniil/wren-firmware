#include "WrenFilesActivity.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

#include "CrossPointSettings.h"
#include "FilesState.h"
#include "SetupUi.h"
#include "activities/util/BmpViewerActivity.h"
#include "wren/ui/WrenDeleteDialog.h"
#include "wren/ui/WrenFonts.h"

namespace {
constexpr int PATH_TOP = 132, ROWS_TOP = 174, ROW_HEIGHT = 64, FOOTER = 80;
bool visibleName(const char* name) {
  return (SETTINGS.showHiddenFiles || name[0] != '.') && strcmp(name, "System Volume Information") != 0;
}
}  // namespace
WrenFilesActivity::WrenFilesActivity(GfxRenderer& r, MappedInputManager& i, const std::string& p)
    : Activity("WrenFiles", r, i), path(p) {}
void WrenFilesActivity::onEnter() {
  Activity::onEnter();
  RenderLock lock;
  values.reserve(visibleRows());
  loadFiles();
  updateWindow();
  requestUpdate();
}
int WrenFilesActivity::count() const { return static_cast<int>(entries.size()) + (path != "/"); }
int WrenFilesActivity::visibleRows() const {
  return std::max(0, (renderer.getScreenHeight() - FOOTER - ROWS_TOP) / ROW_HEIGHT);
}
const std::string& WrenFilesActivity::entryAt(int row) const { return entries[row - (path != "/")]; }
bool WrenFilesActivity::isFile() const {
  return selected >= (path != "/") && selected < count() && !wren::files::directory(entryAt(selected));
}
std::string WrenFilesActivity::selectedPath() const { return path + (path == "/" ? "" : "/") + entryAt(selected); }
void WrenFilesActivity::loadFiles() {
  // Drop the previous directory before reserving the two-pass count. Like the
  // upstream browser, strings own only this directory, not the directory tree.
  std::vector<std::string>().swap(entries);
  cachedStart = -1;
  auto dir = Storage.open(path.c_str());
  if (!dir || !dir.isDirectory()) {
    LOG_ERR("WFILES", "Cannot list %s", path.c_str());
    return;
  }
  size_t total = 0;
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) ++total;
  dir.rewindDirectory();
  entries.reserve(total);
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    file.getName(nameBuffer, sizeof(nameBuffer));
    if (!visibleName(nameBuffer)) continue;
    entries.emplace_back(nameBuffer);
    if (file.isDirectory()) entries.back() += '/';
  }
  std::sort(entries.begin(), entries.end(), wren::files::less);
  selected = wren::files::clampSelection(selected, count());
}
void WrenFilesActivity::updateWindow() {
  const int visible = visibleRows();
  start = wren::files::scrollStart(start, selected, visible, count());
  if (cachedStart == start && cachedVisible == visible) return;
  // At most one screen of stats/counts. No per-folder count map accumulates.
  values.resize(std::min(visible, std::max(0, count() - start)));
  for (int i = 0; i < static_cast<int>(values.size()); ++i) {
    values[i] = 0;
    const int row = start + i;
    if (path != "/" && row == 0) continue;
    const std::string full = path + (path == "/" ? "" : "/") + entryAt(row);
    auto file = Storage.open(full.c_str());
    if (!file) continue;
    if (!file.isDirectory()) {
      values[i] = file.fileSize64();
      continue;
    }
    for (auto child = file.openNextFile(); child; child = file.openNextFile()) {
      child.getName(nameBuffer, sizeof(nameBuffer));
      if (visibleName(nameBuffer)) ++values[i];
    }
  }
  cachedStart = start;
  cachedVisible = visible;
}
void WrenFilesActivity::up() {
  if (path == "/") {
    finish();
    return;
  }
  const auto slash = path.find_last_of('/');
  path.resize(slash == 0 ? 1 : slash);
  selected = 0;
  start = 0;
  loadFiles();
  updateWindow();
  requestUpdate();
}
void WrenFilesActivity::openSelected() {
  if (count() == 0) return;
  if (path != "/" && selected == 0) {
    RenderLock lock;
    up();
    return;
  }
  const std::string full = selectedPath();
  if (wren::files::directory(entryAt(selected))) {
    RenderLock lock;
    path = full.substr(0, full.size() - 1);
    selected = 0;
    start = 0;
    loadFiles();
    updateWindow();
    requestUpdate();
    return;
  }
  if (FsHelpers::hasReflowableBookExtension(full) || FsHelpers::hasXtcExtension(full)) {
    activityManager.goToReader(full);
    return;
  }
  if (FsHelpers::hasBmpExtension(full)) {
    // The manager owns the viewer while this directory's bounded state is retained.
    auto child = makeUniqueNoThrow<BmpViewerActivity>(renderer, mappedInput, full);
    if (!child) {
      LOG_ERR("WFILES", "OOM: image viewer");
      return;
    }
    startActivityForResult(std::move(child), [this](const ActivityResult&) {
      RenderLock lock;
      fullRefresh = true;
    });
  }
}
void WrenFilesActivity::deleteSelected() {
  if (!isFile()) return;
  // Paint the selected row before the child overlays the existing framebuffer.
  requestUpdateAndWait();
  auto dialog = makeUniqueNoThrow<WrenDeleteDialog>(
      renderer, mappedInput, selectedPath(), FsHelpers::getFileNameWithoutExtension(entryAt(selected)),
      [](void* context) { static_cast<WrenFilesActivity*>(context)->drawPage(); }, this);
  if (!dialog) {
    LOG_ERR("WFILES", "OOM: delete dialog");
    return;
  }
  startActivityForResult(std::move(dialog), [this](const ActivityResult& result) {
    RenderLock lock;
    if (!result.isCancelled) {
      loadFiles();
      selected = wren::files::clampSelection(selected, count());
    }
    fullRefresh = true;
    updateWindow();
  });
}
void WrenFilesActivity::loop() {
  using B = MappedInputManager::Button;
  RenderLock lock;
  updateWindow();
  if (isFile() && mappedInput.wasLongPressed(B::Back, 1000)) {
    lock.unlock();
    deleteSelected();
    return;
  }
  int x = 0, y = 0;
  const bool headerBack = mappedInput.wasScreenTapped(x, y) && y >= 28 && y < 68;
  if (mappedInput.wasReleased(B::Back) || mappedInput.wasBackGesture() || headerBack) {
    up();
    return;
  }
  if (wren::setup::swipeSelection(mappedInput, selected, count(), visibleRows())) {
    updateWindow();
    requestUpdate();
    return;
  }
  int row = 0;
  const auto touch = mappedInput.rowTouch(row, ROWS_TOP, ROW_HEIGHT, values.size());
  if (touch != MappedInputManager::RowTouch::None) {
    selected = start + row;
    requestUpdate();
  }
  if (mappedInput.wasScreenLongPress(x, y) && y >= ROWS_TOP &&
      y < ROWS_TOP + static_cast<int>(values.size()) * ROW_HEIGHT) {
    selected = start + (y - ROWS_TOP) / ROW_HEIGHT;
    if (isFile()) {
      lock.unlock();
      deleteSelected();
      return;
    }
  }
  if (mappedInput.wasReleased(B::Confirm) || touch == MappedInputManager::RowTouch::Tap) {
    lock.unlock();
    openSelected();
    return;
  }
  const auto move = [this](int delta) {
    if (count()) selected = (selected + delta + count()) % count();
    updateWindow();
    requestUpdate();
  };
  navigator.onPressAndContinuous({B::Left}, [&move] { move(-1); });
  navigator.onPressAndContinuous({B::Right}, [&move] { move(1); });
}
void WrenFilesActivity::drawPage() {
  using namespace wren::ui;
  wren::setup::status(renderer);
  drawHeader(renderer, 28, tr(STR_WREN_TAB_SETUP), tr(STR_WREN_SETUP_FILES));
  // Measure null-terminated suffixes directly; no path copy on repaint.
  auto tail = wren::files::pathTail(path, 220);
  const int ellipsisWidth = textWidth(renderer, WREN_MONO_16_FONT_ID, "…");
  const int available = renderer.getScreenWidth() - 40;
  while (!tail.empty() &&
         textWidth(renderer, WREN_MONO_16_FONT_ID, tail.data()) + (tail.size() < path.size() ? ellipsisWidth : 0) >
             available)
    tail = wren::files::pathTail(tail, tail.size() - 1);
  int px = 20;
  if (tail.size() < path.size()) {
    drawTextFit(renderer, WREN_MONO_16_FONT_ID, px, PATH_TOP + 10, "…", available);
    px += ellipsisWidth;
  }
  drawTextFit(renderer, WREN_MONO_16_FONT_ID, px, PATH_TOP + 10, tail.data(), renderer.getScreenWidth() - 20 - px);
  renderer.fillRect(0, ROWS_TOP - 3, renderer.getScreenWidth(), 3, true);
  for (int i = 0; i < static_cast<int>(values.size()); ++i) {
    const int row = start + i, y = ROWS_TOP + i * ROW_HEIGHT;
    const bool ink = row != selected, parent = path != "/" && row == 0;
    const auto kind = parent ? wren::files::IconKind::Folder : wren::files::iconKind(entryAt(row));
    const Icon* icon = &icons::BookFile;
    switch (kind) {
      case wren::files::IconKind::Folder:
        icon = &icons::Folder;
        break;
      case wren::files::IconKind::Book:
        break;
      case wren::files::IconKind::Image:
        icon = &icons::ImageFile;
        break;
    }
    renderer.fillRect(0, y, renderer.getScreenWidth(), ROW_HEIGHT - 2, !ink);
    drawIcon(renderer, *icon, 20, y + 15, 1, ink);
    char value[32];
    if (parent)
      snprintf(value, sizeof(value), "%s", tr(STR_WREN_FILES_UP));
    else if (kind == wren::files::IconKind::Folder)
      snprintf(value, sizeof(value), "%llu", static_cast<unsigned long long>(values[i]));
    else
      wren::files::formatSize(value, sizeof(value), values[i]);
    const int right = renderer.getScreenWidth() - 20 - labelWidth(renderer, value);
    drawLabel(renderer, right, y + (62 - renderer.getTextHeight(WREN_MONO_16_FONT_ID)) / 2, value, ink);
    const char* name = parent ? ".." : entryAt(row).c_str();
    // Folder suffix is an internal type tag, not part of the displayed name.
    if (!parent && kind == wren::files::IconKind::Folder) {
      snprintf(nameBuffer, sizeof(nameBuffer), "%.*s", static_cast<int>(entryAt(row).size() - 1), name);
      name = nameBuffer;
    }
    drawTextFit(renderer, WREN_GOLOS_24_FONT_ID, 68, y + (62 - renderer.getTextHeight(WREN_GOLOS_24_FONT_ID)) / 2, name,
                right - 84, ink, -1);
    renderer.fillRect(0, y + 62, renderer.getScreenWidth(), 2, true);
  }
  if (isFile()) {
    const char* hint = tr(STR_WREN_FILES_HOLD_BACK);
    drawLabel(renderer, std::max(20, (renderer.getScreenWidth() - labelWidth(renderer, hint)) / 2),
              renderer.getScreenHeight() - 70, hint);
  }
  wren::setup::footer(renderer);
}
void WrenFilesActivity::render(RenderLock&&) {
  drawPage();
  renderer.displayBuffer(fullRefresh ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  fullRefresh = false;
}
