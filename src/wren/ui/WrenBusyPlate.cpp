#include "WrenBusyPlate.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cstring>

#include "WrenChrome.h"
#include "WrenFonts.h"

namespace wren::ui {
namespace {
constexpr int FRAME = 3;
constexpr int SPRITE = 96;
constexpr int HEIGHT = FRAME * 2 + 20 * 2 + SPRITE;
constexpr int TEXT_X = FRAME + 16 + SPRITE + 20;
constexpr int TEXT_RIGHT = FRAME + 24;

void drawFittedLabel(const GfxRenderer& renderer, int x, int y, const char* label, int maxWidth) {
  if (maxWidth <= 0) return;
  char buffer[240];
  size_t length = std::min(std::strlen(label), sizeof(buffer) - 4);
  while (length && (static_cast<unsigned char>(label[length]) & 0xC0) == 0x80) --length;
  std::memcpy(buffer, label, length);
  buffer[length] = '\0';
  if (label[length] || labelWidth(renderer, buffer) > maxWidth) {
    do {
      if (length) {
        --length;
        while (length && (static_cast<unsigned char>(buffer[length]) & 0xC0) == 0x80) --length;
      }
      std::memcpy(buffer + length, "...", 4);
    } while (length && labelWidth(renderer, buffer) > maxWidth);
    if (labelWidth(renderer, buffer) > maxWidth) return;
  }
  drawLabel(renderer, x, y, buffer);
}

Rect cardRect(const GfxRenderer& renderer) {
  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();
  const int width = std::max(0, std::min(448, screenWidth - 64));
  const int y = screenHeight >= screenWidth ? 252 : (screenHeight - HEIGHT) / 2;
  return Rect{(screenWidth - width) / 2, y, width, HEIGHT};
}
}  // namespace

Rect busyPlateBounds(const GfxRenderer& renderer) {
  const Rect card = cardRect(renderer);
  return Rect{card.x - 8, card.y - 8, card.width + 24, card.height + 24};
}

Rect drawBusyPlate(const GfxRenderer& renderer, BusyPose pose, const char* headline, const char* label) {
  const Rect card = cardRect(renderer);
  const int x = card.x;
  const int y = card.y;
  const int width = card.width;
  renderer.fillRect(x - 8, y - 8, width + 24, HEIGHT + 24, false);
  renderer.fillRect(x + 8, y + 8, width, HEIGHT, true);
  renderer.fillRect(x, y, width, HEIGHT, false);
  renderer.drawRect(x, y, width, HEIGHT, FRAME, true);

  const Icon* icon = &icons::WrenHello;
  switch (pose) {
    case BusyPose::Wait:
      icon = &icons::WrenHello;
      break;
    case BusyPose::Sleep:
      icon = &icons::WrenSleeping;
      break;
    case BusyPose::Error:
      icon = &icons::WrenConfused;
      break;
  }
  drawIcon(renderer, *icon, x + FRAME + 16, y + FRAME + 20, 3);
  const int textX = x + TEXT_X;
  const int textWidthAvailable = std::max(0, width - TEXT_X - TEXT_RIGHT);
  const char* text = headline ? headline : "";
  const bool wrapped = textWidth(renderer, WREN_GOLOS_40_FONT_ID, text) > textWidthAvailable;
  const int lines = wrapped ? wrappedLineCount(renderer, WREN_GOLOS_24_FONT_ID, text, textWidthAvailable, 2) : 1;
  const int headlineHeight = wrapped ? lines * 28 : 44;
  const bool hasLabel = label && *label;
  const int blockHeight = headlineHeight + (hasLabel ? 4 + 20 : 0);
  const int textY = y + (HEIGHT - blockHeight) / 2;
  if (wrapped) {
    drawTextWrapped(renderer, WREN_GOLOS_24_FONT_ID, textX, textY, text, textWidthAvailable, 2, 28);
  } else {
    drawTextFit(renderer, WREN_GOLOS_40_FONT_ID, textX, textY, text, textWidthAvailable);
  }
  if (hasLabel) drawFittedLabel(renderer, textX, textY + headlineHeight + 4, label, textWidthAvailable);
  return card;
}

void drawBusyProgress(const GfxRenderer& renderer, const Rect& card, int percent) {
  const int x = card.x + TEXT_X;
  const int y = card.y + (card.height - 44) / 2 + 44 + 12;
  const int width = std::max(0, card.width - TEXT_X - TEXT_RIGHT);
  if (width <= 4) return;
  renderer.fillRect(x, y, width, 14, false);
  renderer.drawRect(x, y, width, 14, 2, true);
  renderer.fillRect(x + 2, y + 2, (width - 4) * std::clamp(percent, 0, 100) / 100, 10, true);
}
}  // namespace wren::ui
