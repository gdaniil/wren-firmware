#include "WrenChrome.h"

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "WrenFonts.h"
#include "WrenLayout.h"

namespace wren::ui {
namespace {
constexpr int LABEL = WREN_MONO_16_FONT_ID;
constexpr int ROW = WREN_GOLOS_24_FONT_ID;
constexpr int TITLE = WREN_GOLOS_40_FONT_ID;
constexpr size_t CAP = 240;

// Copies complete UTF-8 sequences and uppercases the supported two-byte letters.
void copyText(char (&out)[CAP], const char* in, bool upper) {
  size_t n = 0;
  if (!in) {
    out[0] = 0;
    return;
  }
  while (*in && n + 8 <= CAP) {
    unsigned char a = static_cast<unsigned char>(*in++);
    if (a < 0x80) {
      out[n++] = upper && a >= 'a' && a <= 'z' ? a - 32 : a;
    } else {
      int len = (a & 0xe0) == 0xc0 ? 2 : (a & 0xf0) == 0xe0 ? 3 : (a & 0xf8) == 0xf0 ? 4 : 1;
      unsigned char bytes[4] = {a, 0, 0, 0};
      int count = 1;
      while (count < len && *in && (static_cast<unsigned char>(*in) & 0xc0) == 0x80) bytes[count++] = *in++;
      if (upper && count == 2) {
        unsigned cp = ((bytes[0] & 31) << 6) | (bytes[1] & 63);
        if (cp >= 0x430 && cp <= 0x44f)
          cp -= 0x20;
        else if (cp >= 0x450 && cp <= 0x45f)
          cp -= 0x50;
        else if (cp == 0x491)
          cp = 0x490;
        bytes[0] = 0xc0 | (cp >> 6);
        bytes[1] = 0x80 | (cp & 63);
      }
      for (int i = 0; i < count; ++i) out[n++] = bytes[i];
    }
  }
  if (*in) {
    memcpy(out + n, "\xe2\x80\xa6", 3);
    n += 3;
  }
  out[n] = 0;
}
int width(const GfxRenderer& r, int font, const char* text, int tracking) {
  return r.getTextAdvanceX(font, text, EpdFontFamily::REGULAR, tracking, BidiUtils::BidiBaseDir::AUTO,
                           GfxRenderer::TextMeasureMode::Rendered);
}
void draw(const GfxRenderer& r, int font, int x, int y, const char* text, bool ink, int tracking) {
  r.drawText(font, x, y, text, ink, EpdFontFamily::REGULAR, BidiUtils::BidiBaseDir::AUTO, tracking);
}
void fit(const GfxRenderer& r, char (&s)[CAP], int font, int tracking, int maxWidth) {
  if (width(r, font, s, tracking) <= maxWidth) return;
  size_t n = strlen(s);
  while (n) {
    do {
      --n;
    } while (n && (static_cast<unsigned char>(s[n]) & 0xc0) == 0x80);
    s[n] = 0;
    if (n + 3 < CAP) {
      memcpy(s + n, "\xe2\x80\xa6", 4);
      if (width(r, font, s, tracking) <= maxWidth) return;
      s[n] = 0;
    }
  }
  if (width(r, font, "\xe2\x80\xa6", tracking) <= maxWidth) strcpy(s, "\xe2\x80\xa6");
}
float clampFraction(float f) { return std::isfinite(f) ? std::max(0.0f, std::min(1.0f, f)) : 0.0f; }
void drawBattery(const GfxRenderer& r, int x, int y, int percent) {
  r.fillRect(x, y, 25, 11, false);
  r.drawRect(x, y, 22, 11, 2, true);
  r.fillRect(x + 3, y + 3, 16 * percent / 100, 5, true);
  r.fillRect(x + 23, y + 3, 2, 5, true);
}
}  // namespace

void drawIcon(const GfxRenderer& r, const Icon& icon, int x, int y, int scale, bool ink) {
  if (scale <= 0 || icon.width > 32 || !icon.rows) return;
  for (int j = 0; j < icon.height; ++j)
    for (int i = 0; i < icon.width; ++i)
      if (icon.rows[j] & (uint32_t{1} << (icon.width - 1 - i)))
        r.fillRect(x + i * scale, y + j * scale, scale, scale, ink);
}
int textWidth(const GfxRenderer& r, int font, const char* text, int tracking) {
  return text && *text ? width(r, font, text, tracking) : 0;
}
void drawTextFit(const GfxRenderer& r, int font, int x, int y, const char* text, int maxWidth, bool ink, int tracking) {
  if (maxWidth <= 0) return;
  char s[CAP];
  copyText(s, text, false);
  fit(r, s, font, tracking, maxWidth);
  draw(r, font, x, y, s, ink, tracking);
}
namespace {
// Lays out word-wrapped text; draws it when doDraw. Returns the number of lines.
int wrap(const GfxRenderer& r, int font, int x, int y, const char* text, int maxWidth, int maxLines, int lineStep,
         bool ink, int tracking, bool doDraw) {
  if (!text || !*text || maxLines <= 0 || maxWidth <= 0) return 0;
  char all[CAP];
  copyText(all, text, false);
  const char* p = all;
  int lines = 0;
  while (*p && lines < maxLines) {
    while (*p == ' ') ++p;
    if (!*p) break;
    char line[CAP];
    size_t used = 0;
    const char* q = p;
    // Take whole words while the line still fits.
    while (*q) {
      const char* end = q;
      while (*end && *end != ' ') ++end;
      const size_t start = used;
      if (used) line[used++] = ' ';
      const size_t len = std::min<size_t>(end - q, CAP - 1 - used);
      memcpy(line + used, q, len);
      line[used + len] = 0;
      if (start && width(r, font, line, tracking) > maxWidth) {
        line[start] = 0;
        used = start;
        break;
      }
      used += len;
      q = end;
      while (*q == ' ') ++q;
      if (!start && width(r, font, line, tracking) > maxWidth) break;  // one word wider than the line
    }
    line[used] = 0;
    // On the last line keep the rest of the text so fit() cuts it with an ellipsis.
    if (lines == maxLines - 1 && *q) copyText(line, p, false);
    if (doDraw) {
      fit(r, line, font, tracking, maxWidth);
      draw(r, font, x, y + lines * lineStep, line, ink, tracking);
    }
    ++lines;
    p = q;
  }
  return lines;
}
}  // namespace
int drawTextWrapped(const GfxRenderer& r, int font, int x, int y, const char* text, int maxWidth, int maxLines,
                    int lineStep, bool ink, int tracking) {
  return wrap(r, font, x, y, text, maxWidth, maxLines, lineStep, ink, tracking, true);
}
int wrappedLineCount(const GfxRenderer& r, int font, const char* text, int maxWidth, int maxLines, int tracking) {
  return wrap(r, font, 0, 0, text, maxWidth, maxLines, 0, true, tracking, false);
}
int labelWidth(const GfxRenderer& r, const char* text) {
  char upper[CAP];
  copyText(upper, text, true);
  return width(r, LABEL, upper, 1);
}
void drawLabel(const GfxRenderer& r, int x, int y, const char* text, bool ink) {
  char upper[CAP];
  copyText(upper, text, true);
  draw(r, LABEL, x, y, upper, ink, 1);
}
void drawMenuStatusBar(const GfxRenderer& r, const char* clock, int percent) {
  r.fillRect(0, 0, r.getScreenWidth(), 28, false);
  char text[CAP];
  copyText(text, clock, true);
  draw(r, LABEL, 20, 6, text, true, 1);
  percent = std::max(0, std::min(100, percent));
  char number[8];
  const bool showNumber = SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_ALWAYS;
  if (!showNumber) {
    drawBattery(r, r.getScreenWidth() - 20 - 25, 8, percent);
    return;
  }
  snprintf(number, sizeof(number), "%d%%", percent);
  int tx = r.getScreenWidth() - 20 - width(r, LABEL, number, 1);
  draw(r, LABEL, tx, 6, number, true, 1);
  int bx = tx - 8 - 25;
  drawBattery(r, bx, 8, percent);
}
void drawReaderStatusLine(const GfxRenderer& r, const char* clock, float fraction, int percent, int y) {
  r.fillRect(0, y, r.getScreenWidth(), 40, false);
  percent = std::max(0, std::min(100, percent));
  char number[8];
  const int textY = y + (40 - r.getTextHeight(LABEL)) / 2;
  // The reader hides the number for both "in reader" and "always".
  const bool showNumber = SETTINGS.hideBatteryPercentage == CrossPointSettings::HIDE_NEVER;
  int bx = r.getScreenWidth() - 32 - 25;
  if (showNumber) {
    snprintf(number, sizeof(number), "%d%%", percent);
    const int tx = r.getScreenWidth() - 32 - width(r, LABEL, number, 1);
    draw(r, LABEL, tx, textY, number, true, 1);
    bx = tx - 12 - 25;
  }
  drawBattery(r, bx, y + (40 - 11) / 2, percent);
  int left = 32;
  if (clock && *clock) {
    char text[CAP];
    copyText(text, clock, true);
    draw(r, LABEL, left, textY, text, true, 1);
    left += width(r, LABEL, text, 1) + 16;
  }
  drawProgressLine(r, left, y + (40 - 6) / 2, bx - 16 - left, fraction);
}
int drawHeader(const GfxRenderer& r, int y, const char* crumb, const char* title, const char* rightLabel) {
  char text[CAP];
  if (crumb && *crumb) {
    drawIcon(r, icons::ChevronLeft, 20, y + 12, 2, true);
    copyText(text, crumb, true);
    fit(r, text, LABEL, 1, r.getScreenWidth() - 54);
    draw(r, LABEL, 34, y + 12, text, true, 1);
  }
  copyText(text, title, false);
  const int labelSpace = rightLabel && *rightLabel ? labelWidth(r, rightLabel) + 16 : 0;
  fit(r, text, TITLE, -2, std::max(0, r.getScreenWidth() - 40 - labelSpace));
  draw(r, TITLE, 20, y + 101 - 12 - r.getFontAscenderSize(TITLE), text, true, -2);
  if (rightLabel && *rightLabel) {
    drawLabel(r, r.getScreenWidth() - 20 - labelWidth(r, rightLabel), y + 101 - 12 - 6 - r.getFontAscenderSize(LABEL),
              rightLabel);
  }
  r.fillRect(0, y + 101, r.getScreenWidth(), 3, true);
  return y + 104;
}
int drawBookHeader(const GfxRenderer& r, int y, const char* title, int page, int total, int percent) {
  char text[CAP];
  copyText(text, title, false);
  fit(r, text, TITLE, -2, r.getScreenWidth() - 40);
  const int lineY = y + 101 - 14 - r.getTextHeight(LABEL);
  const int barY = lineY - 8 - 8;
  draw(r, TITLE, 20, barY - 12 - r.getTextHeight(TITLE), text, true, -2);
  drawProgressBar(r, 20, barY, r.getScreenWidth() - 40, percent / 100.0f);
  if (total > 0) {
    snprintf(text, sizeof(text), "%d/%d", page, total);
    draw(r, LABEL, 20, lineY, text, true, 1);
  }
  snprintf(text, sizeof(text), "%d%%", percent);
  draw(r, LABEL, r.getScreenWidth() - 20 - width(r, LABEL, text, 1), lineY, text, true, 1);
  r.fillRect(0, y + 101, r.getScreenWidth(), 3, true);
  return y + 104;
}
void drawTabHeader(const GfxRenderer& r, int top, int activeTab) {
  const int sw = r.getScreenWidth();
  drawIcon(r, icons::Wren, 20, top + 8, 2, true);
  const int x = 100, w = sw - 20 - x, y = top + 12;
  r.drawRect(x, y, w, 56, 3, true);
  const StrId labels[] = {StrId::STR_WREN_TAB_LIBRARY, StrId::STR_WREN_TAB_SEND, StrId::STR_WREN_TAB_SETUP};
  const int cell = (w - 12) / 3;
  for (int i = 0; i < 3; ++i) {
    const int cx = x + 3 + i * (cell + 3);
    const int cw = i == 2 ? x + w - 3 - cx : cell;
    if (i) r.fillRect(cx - 3, y, 3, 56, true);
    const bool current = i == activeTab;
    if (current) r.fillRect(cx, y + 3, cw, 50, true);
    const char* label = I18N.get(labels[i]);
    int font = WREN_GOLOS_32_FONT_ID;
    if (textWidth(r, font, label, -1) > cw - 8) font = WREN_GOLOS_24_FONT_ID;
    drawTextFit(r, font, cx + (cw - std::min(cw - 8, textWidth(r, font, label, -1))) / 2,
                y + (56 - r.getTextHeight(font)) / 2, label, cw - 8, !current, -1);
  }
}
int drawRow(const GfxRenderer& r, int y, const Row& row) {
  const int sw = r.getScreenWidth();
  bool ink = !row.selected;
  r.fillRect(0, y, sw, layout::ROW_HEIGHT - 2, row.selected);
  int right = sw - 20;
  if (row.marker == Marker::Chevron || row.marker == Marker::Icon) {
    right -= 24;
    const Icon* icon = row.marker == Marker::Chevron ? &icons::ChevronRight : row.icon;
    int scale = row.marker == Marker::Chevron ? 2 : row.iconScale;
    if (icon && scale > 0)
      drawIcon(r, *icon, right + (24 - icon->width * scale) / 2, y + (52 - icon->height * scale) / 2, scale, ink);
    right -= 16;
  } else if (row.marker == Marker::Toggle) {
    right -= 48;
    r.drawRect(right, y + 13, 48, 26, 3, ink);
    if (row.on) r.fillRect(right + 3, y + 16, 42, 20, ink);
    r.fillRect(right + (row.on ? 28 : 6), y + 19, 14, 14, row.on ? !ink : ink);
    right -= 16;
  } else if (row.marker == Marker::Options) {
    int count = std::max(0, std::min(5, row.optionCount));
    if (count) {
      right -= count * 14 + (count - 1) * 6;
      for (int i = 0; i < count; ++i) {
        int x = right + i * 20;
        r.drawRect(x, y + 19, 14, 14, 2, ink);
        if (i == row.chosenOption) r.fillRect(x + 2, y + 21, 10, 10, ink);
      }
      right -= 16;
    }
  }
  char text[CAP];
  if (row.value && *row.value) {
    copyText(text, row.value, row.uppercaseValue);
    fit(r, text, LABEL, 1, std::max(0, right - 20));
    int w = width(r, LABEL, text, 1);
    right -= w;
    draw(r, LABEL, right, y + (52 - r.getTextHeight(LABEL)) / 2, text, ink, 1);
    right -= 16;
  }
  copyText(text, row.label, false);
  fit(r, text, ROW, -1, layout::truncationWidth(20, right));
  draw(r, ROW, 20, y + (52 - r.getTextHeight(ROW)) / 2, text, ink, -1);
  r.fillRect(0, y + 52, sw, 2, true);
  return y + layout::ROW_HEIGHT;
}
void drawChapterRow(const GfxRenderer& r, int y, const char* number, const char* title, const char* value,
                    int numberWidth, bool selected) {
  const int sw = r.getScreenWidth();
  const bool ink = !selected;
  r.fillRect(0, y, sw, 60, selected);
  drawLabel(r, 20, y + (60 - r.getTextHeight(LABEL)) / 2, number, ink);
  int right = sw - 20 - labelWidth(r, value);
  drawLabel(r, right, y + (60 - r.getTextHeight(LABEL)) / 2, value, ink);
  int left = 20 + numberWidth + 16;
  char text[CAP];
  copyText(text, title, false);
  fit(r, text, ROW, -1, std::max(0, right - 16 - left));
  draw(r, ROW, left, y + (60 - r.getTextHeight(ROW)) / 2, text, ink, -1);
  r.fillRect(0, y + 60, sw, 2, true);
}
void drawButtonTabs(GfxRenderer& r, const Tab (&labels)[4], bool physicalEdge) {
  if (gpio.hasTouch()) return;
  auto old = r.getOrientation();
  if (physicalEdge) r.setOrientation(GfxRenderer::Portrait);
  int y = r.getScreenHeight() - 30;
  const bool grayscale = r.getRenderMode() != GfxRenderer::BW && !r.grayPlanesAreAbsolute();
  for (int i = 0; i < 4; ++i) {
    const Tab& tab = labels[i];
    if (tab.kind == TabKind::Empty || (tab.kind == TabKind::Text && (!tab.text || !*tab.text))) continue;
    int x = layout::tabX(r.getScreenWidth(), i);
    r.fillRect(x, y, 106, 30, grayscale);
    if (grayscale) continue;
    r.fillRect(x, y, 106, 3, true);
    r.fillRect(x, y, 3, 30, true);
    r.fillRect(x + 103, y, 3, 30, true);
    if (tab.kind == TabKind::Text) {
      char text[CAP];
      copyText(text, tab.text, true);
      fit(r, text, LABEL, 1, 100);
      draw(r, LABEL, x + (106 - width(r, LABEL, text, 1)) / 2, y + (30 - r.getTextHeight(LABEL)) / 2, text, true, 1);
    } else {
      const Icon& icon = tab.kind == TabKind::ArrowUp ? icons::ArrowUp : icons::ArrowDown;
      drawIcon(r, icon, x + (106 - icon.width * 2) / 2, y + (30 - icon.height * 2) / 2, 2, true);
    }
  }
  r.setOrientation(old);
}
void drawEdgeTabs(const GfxRenderer& r, bool left, bool right) {
  if (gpio.hasTouch() || !gpio.hasEdgeSideButtons()) return;
  for (int i = 0; i < 2; ++i) {
    if (!(i ? right : left)) continue;
    int x = i ? r.getScreenWidth() - 14 : 0;
    r.fillRect(x, 155, 14, 80, false);
    r.fillRect(x, 155, 14, 3, true);
    r.fillRect(x, 232, 14, 3, true);
    r.fillRect(i ? x : x + 11, 155, 3, 80, true);
    drawIcon(r, i ? icons::ChevronRight : icons::ChevronLeft, x + 4, 189, 2, true);
  }
}
void drawProgressLine(const GfxRenderer& r, int x, int y, int w, float fraction) {
  if (w < 6) return;
  r.fillRect(x, y, w, 6, false);
  r.fillRect(x, y + 2, w, 2, true);
  r.fillRect(x + static_cast<int>(clampFraction(fraction) * (w - 6)), y, 6, 6, true);
}
void drawProgressBar(const GfxRenderer& r, int x, int y, int w, float fraction) {
  if (w < 4) return;
  r.fillRect(x, y, w, 8, false);
  r.drawRect(x, y, w, 8, 2, true);
  float f = clampFraction(fraction);
  if (f > 0) r.fillRect(x + 2, y + 2, std::min(w - 4, std::max(6, static_cast<int>((w - 4) * f))), 4, true);
}
}  // namespace wren::ui
