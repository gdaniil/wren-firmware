#include "WrenTheme.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstring>

#include "WrenBusyPlate.h"

Rect WrenTheme::drawPopup(const GfxRenderer& renderer, const char* message) const {
  using wren::ui::BusyPose;
  BusyPose pose = BusyPose::Wait;
  const char* headline = message;
  const char* label = nullptr;
  if (message && std::strcmp(message, tr(STR_ENTERING_SLEEP)) == 0) {
    pose = BusyPose::Sleep;
    headline = tr(STR_WREN_BUSY_SLEEP);
  } else if (message && std::strcmp(message, tr(STR_LOADING_POPUP)) == 0) {
    headline = tr(STR_WREN_BUSY_WAIT);
  } else if (message && std::strcmp(message, tr(STR_INDEXING)) == 0) {
    headline = tr(STR_WREN_BUSY_INDEXING);
  } else if (message && std::strcmp(message, tr(STR_SAVE_PROGRESS_FAILED)) == 0) {
    pose = BusyPose::Error;
    headline = tr(STR_WREN_BUSY_SAVE_FAILED);
    label = tr(STR_WREN_BUSY_CARD_BUSY);
  } else if (message && std::strcmp(message, tr(STR_FONT_DAMAGED)) == 0) {
    pose = BusyPose::Error;
    headline = tr(STR_WREN_BUSY_FONT_DAMAGED);
    label = tr(STR_WREN_BUSY_DEFAULT_FONT);
  }
  const Rect card = wren::ui::drawBusyPlate(renderer, pose, headline, label);
  renderer.displayBuffer();
  return card;
}

void WrenTheme::fillPopupProgress(const GfxRenderer& renderer, const Rect& layout, int progress) const {
  wren::ui::drawBusyProgress(renderer, layout, progress);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
