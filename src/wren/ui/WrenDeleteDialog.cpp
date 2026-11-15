#include "WrenDeleteDialog.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <I18n.h>
#include <LibraryBuilder.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "RecentBooksStore.h"
#include "WrenChrome.h"
#include "WrenFonts.h"
#include "util/BookCacheUtils.h"
#include "wren/setup/FilesState.h"

WrenDeleteDialog::WrenDeleteDialog(GfxRenderer& r, MappedInputManager& i, const std::string& p, const std::string& t,
                                   void (*paint)(void*), void* context, bool defer)
    : Activity("WrenDelete", r, i),
      path(p),
      title(t),
      paintBackground(paint),
      backgroundContext(context),
      deferRemoval(defer) {}
WrenDeleteDialog::WrenDeleteDialog(GfxRenderer& r, MappedInputManager& i, const char* heading, const char* body,
                                   const char* confirmLabel, void (*paint)(void*), void* context)
    : Activity("WrenDelete", r, i),
      paintBackground(paint),
      backgroundContext(context),
      customHeading(heading),
      customBody(body),
      customConfirm(confirmLabel) {}
void WrenDeleteDialog::onEnter() {
  Activity::onEnter();
  swallowBack = mappedInput.isPressed(MappedInputManager::Button::Back);
  swallowConfirm = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  if (!customHeading) {
    auto file = Storage.open(path.c_str());
    if (file) bytes = file.fileSize64();
  }
  requestUpdate();
}
void WrenDeleteDialog::keep() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}
bool wren::ui::deleteBookFromCard(const std::string& path) {
  bool removed = removeBookFile(path);
  const bool book = FsHelpers::hasReflowableBookExtension(path) || FsHelpers::hasXtcExtension(path);
  if (!removed && !book) removed = Storage.remove(path.c_str());
  // removeBookFile may report clipping cleanup failure after removing the file.
  if (!removed && Storage.exists(path.c_str())) return false;
  if (RECENT_BOOKS.removeByPath(path)) RECENT_BOOKS.saveToFile();
  if (library::isLibraryBookFile(path)) library::markLibraryIndexDirty();
  return true;
}

void WrenDeleteDialog::remove() {
  if (customHeading || deferRemoval) {
    setResult(ActivityResult{});
    finish();
    return;
  }
  RenderLock lock;
  if (!wren::ui::deleteBookFromCard(path)) {
    LOG_ERR("WDELETE", "Failed to delete %s", path.c_str());
    failed = true;
    requestUpdate();
    return;
  }
  setResult(ActivityResult{});
  lock.unlock();
  finish();
}
void WrenDeleteDialog::loop() {
  using B = MappedInputManager::Button;
  const bool back = mappedInput.wasReleased(B::Back);
  const bool confirm = mappedInput.wasReleased(B::Confirm);
  if (swallowBack && !mappedInput.isPressed(B::Back)) {
    swallowBack = false;
    return;
  }
  if (swallowConfirm && !mappedInput.isPressed(B::Confirm)) {
    swallowConfirm = false;
    return;
  }
  int x = 0, y = 0;
  const bool tap = mappedInput.wasScreenTapped(x, y) && y >= renderer.getScreenHeight() - 40;
  if ((!swallowBack && back) || mappedInput.wasBackGesture() || (tap && x < renderer.getScreenWidth() / 2)) {
    keep();
    return;
  }
  if ((!swallowConfirm && confirm) || (tap && x >= renderer.getScreenWidth() / 2)) remove();
}
void WrenDeleteDialog::render(RenderLock&&) {
  using namespace wren::ui;
  const int sw = renderer.getScreenWidth(), sh = renderer.getScreenHeight();
  // A nested control-center visit may overwrite the framebuffer. Repaint the
  // retained parent each time instead of keeping a screen-sized backup.
  paintBackground(backgroundContext);
  for (int y = 0; y < sh - 40; y += 2)
    for (int x = ((y / 2) % 2) * 2; x < sw; x += 4)
      renderer.fillRect(x, y, std::min(2, sw - x), std::min(2, sh - 40 - y), true);
  const int x = 32, w = sw - 64, left = x + 27, content = w - 54;
  const char* bodyText = customBody ? customBody : tr(STR_WREN_FILES_DELETE_BODY);
  const int lines = std::max(1, std::min(3, (sh - 40 - 48 - 64 - 16 - 40 - 12 - 20 - 2 - 12 - 20 - 16) / 25));
  const int bodyHeight = wrappedLineCount(renderer, WREN_GOLOS_18_FONT_ID, bodyText, content, lines) * 25;
  const int h = 27 + 64 + 16 + 40 + 12 + bodyHeight + (customHeading ? 0 : 20 + 2 + 12 + 20) + 27;
  const int y = sw > sh ? (sh - 40 - h) / 2 : std::min(210, sh - 40 - h);
  renderer.fillRect(x, y, w, h, false);
  renderer.drawRect(x, y, w, h, 3, true);
  int cy = y + 27;
  drawIcon(renderer, icons::Trash, left, cy, 2);
  cy += 64 + 16;
  if (customHeading) {
    drawTextFit(renderer, WREN_GOLOS_40_FONT_ID, left, cy, customHeading, content, true, -2);
    cy += 40 + 12;
    drawTextWrapped(renderer, WREN_GOLOS_18_FONT_ID, left, cy, bodyText, content, lines, 25);
  } else {
    // Bound the display copy; the full SD path remains owned by the dialog.
    char heading[192];
    const int length = snprintf(heading, sizeof(heading) - 4, tr(STR_WREN_FILES_DELETE_TITLE), title.c_str());
    heading[utf8SafeTruncateBuffer(heading, strlen(heading))] = '\0';
    if (length >= static_cast<int>(sizeof(heading) - 4)) strcat(heading, "…");
    drawTextFit(renderer, WREN_GOLOS_40_FONT_ID, left, cy, heading, content, true, -2);
    cy += 40 + 12;
    drawTextWrapped(renderer, WREN_GOLOS_18_FONT_ID, left, cy, failed ? tr(STR_WREN_FILES_DELETE_FAILED) : bodyText,
                    content, lines, 25);
    cy += bodyHeight + 20;
    renderer.fillRect(left, cy, content, 2, true);
    cy += 14;
    char size[32];
    wren::files::formatSize(size, sizeof(size), bytes);
    const int sizeWidth = labelWidth(renderer, size);
    const auto slash = path.find_last_of('/');
    const char* filename = path.c_str() + (slash == std::string::npos ? 0 : slash + 1);
    drawTextFit(renderer, WREN_MONO_16_FONT_ID, left, cy, filename, content - sizeWidth - 16);
    drawLabel(renderer, left + content - sizeWidth, cy, size);
  }
  renderer.fillRect(0, sh - 40, sw, 40, false);
  const char* confirmText = customConfirm ? customConfirm : tr(STR_WREN_FILES_DELETE);
  const Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_FILES_KEEP)}, {TabKind::Text, confirmText}, {}, {}};
  drawButtonTabs(renderer, tabs, false);
  if (mappedInput.hasTouch()) {
    drawLabel(renderer, 32, sh - 30, tr(STR_WREN_FILES_KEEP));
    drawLabel(renderer, sw / 2 + 16, sh - 30, confirmText);
  }
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
