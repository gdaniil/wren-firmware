#pragma once

#include "WrenIcons.generated.h"

class GfxRenderer;

namespace wren::ui {
enum class Marker { None, Chevron, Icon, Options };
struct Row {
  const char* label = nullptr;
  const char* value = nullptr;
  Marker marker = Marker::None;
  const Icon* icon = nullptr;
  int iconScale = 1;
  int optionCount = 0;
  int chosenOption = -1;
  bool selected = false;
};
enum class TabKind { Empty, Text, ArrowUp, ArrowDown };
struct Tab {
  TabKind kind = TabKind::Empty;
  const char* text = nullptr;
};
void drawIcon(const GfxRenderer& renderer, const Icon& icon, int x, int y, int scale, bool ink = true);
// Label style: mono 16, uppercase, +1 px tracking. y is the top of the text line.
int labelWidth(const GfxRenderer& renderer, const char* text);
void drawLabel(const GfxRenderer& renderer, int x, int y, const char* text, bool ink = true);
void drawMenuStatusBar(const GfxRenderer& renderer, const char* clock, int batteryPercent);
void drawReaderStatusLine(const GfxRenderer& renderer, const char* clock, float bookFraction, int batteryPercent);
int drawHeader(const GfxRenderer& renderer, int y, const char* crumb, const char* title,
               const char* rightLabel = nullptr);
int drawBookHeader(const GfxRenderer& renderer, int y, const char* title, int page, int total, int percent);
int drawRow(const GfxRenderer& renderer, int y, const Row& row);
void drawChapterRow(const GfxRenderer& renderer, int y, const char* number, const char* title, const char* value,
                    int numberWidth, bool selected);
void drawButtonTabs(GfxRenderer& renderer, const Tab (&labels)[4]);
void drawEdgeTabs(const GfxRenderer& renderer, bool showLeft, bool showRight);
void drawProgressLine(const GfxRenderer& renderer, int x, int y, int w, float fraction);
void drawProgressBar(const GfxRenderer& renderer, int x, int y, int w, float fraction);
}  // namespace wren::ui
