#pragma once

#include "WrenIcons.generated.h"

class GfxRenderer;

namespace wren::ui {
enum class Marker { None, Chevron, Icon, Options, Toggle };
struct Row {
  const char* label = nullptr;
  const char* value = nullptr;
  Marker marker = Marker::None;
  const Icon* icon = nullptr;
  int iconScale = 1;
  int optionCount = 0;
  int chosenOption = -1;
  bool selected = false;
  bool on = false;
  bool uppercaseValue = true;
  const Icon* leadingIcon = nullptr;
};
enum class TabKind { Empty, Text, ArrowUp, ArrowDown };
struct Tab {
  TabKind kind = TabKind::Empty;
  const char* text = nullptr;
};
void drawIcon(const GfxRenderer& renderer, const Icon& icon, int x, int y, int scale, bool ink = true);
// Plain text in any Wren font. y is the top of the text line; tracking in px.
int textWidth(const GfxRenderer& renderer, int font, const char* text, int tracking = 0);
// One line, cut with an ellipsis to maxWidth.
void drawTextFit(const GfxRenderer& renderer, int font, int x, int y, const char* text, int maxWidth, bool ink = true,
                 int tracking = 0);
// Word-wrapped to maxWidth, at most maxLines, ellipsis on the last line. Returns the number of lines drawn.
int drawTextWrapped(const GfxRenderer& renderer, int font, int x, int y, const char* text, int maxWidth, int maxLines,
                    int lineStep, bool ink = true, int tracking = 0);
int wrappedLineCount(const GfxRenderer& renderer, int font, const char* text, int maxWidth, int maxLines,
                     int tracking = 0);
// Label style: mono 16, uppercase, +1 px tracking. y is the top of the text line.
int labelWidth(const GfxRenderer& renderer, const char* text);
void drawLabel(const GfxRenderer& renderer, int x, int y, const char* text, bool ink = true);
void drawMenuStatusBar(const GfxRenderer& renderer, const char* clock, int batteryPercent);
// y is the top of the 40 px band (0 for EPUB; XTC may draw it at the bottom).
void drawReaderStatusLine(const GfxRenderer& renderer, const char* clock, float bookFraction, int batteryPercent,
                          int y = 0);
int drawHeader(const GfxRenderer& renderer, int y, const char* crumb, const char* title,
               const char* rightLabel = nullptr);
int drawBookHeader(const GfxRenderer& renderer, int y, const char* title, int page, int total, int percent);
void drawTabHeader(const GfxRenderer& renderer, int y, int activeTab);
int drawRow(const GfxRenderer& renderer, int y, const Row& row);
void drawChapterRow(const GfxRenderer& renderer, int y, const char* number, const char* title, const char* value,
                    int numberWidth, bool selected);
void drawButtonTabs(GfxRenderer& renderer, const Tab (&labels)[4], bool physicalEdge = true);
void drawEdgeTabs(const GfxRenderer& renderer, bool showLeft, bool showRight);
void drawProgressLine(const GfxRenderer& renderer, int x, int y, int w, float fraction);
void drawProgressBar(const GfxRenderer& renderer, int x, int y, int w, float fraction);
}  // namespace wren::ui
