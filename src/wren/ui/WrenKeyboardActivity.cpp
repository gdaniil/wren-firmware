#include "WrenKeyboardActivity.h"

#include <BidiUtils.h>
#include <I18n.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "HapticFeedback.h"
#include "MappedInputManager.h"
#include "activities/util/KeyboardLayoutSet.h"
#include "wren/setup/SetupUi.h"
#include "wren/ui/WrenFonts.h"

namespace fui = freeink::ui;

namespace {

wren::ui::keyboard::LanguageKey languageKey(fui::KeyboardLayoutId id) {
  using K = wren::ui::keyboard::LanguageKey;
  switch (id) {
    case fui::KeyboardLayoutId::QwertyEn:
      return K::En;
    case fui::KeyboardLayoutId::AzertyFr:
      return K::Fr;
    case fui::KeyboardLayoutId::QwertzDe:
      return K::De;
    case fui::KeyboardLayoutId::SpanishEs:
      return K::Es;
    case fui::KeyboardLayoutId::CyrillicRu:
      return K::Ru;
    case fui::KeyboardLayoutId::CyrillicUk:
      return K::Uk;
    case fui::KeyboardLayoutId::CyrillicBe:
      return K::Be;
    case fui::KeyboardLayoutId::CyrillicKk:
      return K::Kk;
    case fui::KeyboardLayoutId::HebrewIl:
      return K::He;
    case fui::KeyboardLayoutId::ArabicAr:
      return K::Ar;
    case fui::KeyboardLayoutId::GreekEl:
      return K::El;
  }
  return K::En;
}

constexpr fui::ActionId ACTION_KEY = 1;
// ---------------------------------------------------------------------------
// URL layers. The SDK builtin layouts have no URL variant (":", "/", ".", the
// snippet panel), so these are app-defined tables over the same public
// KeyboardLayout structs. URLs are ASCII, so the letter rows are EN-arranged
// regardless of UI language.
// ---------------------------------------------------------------------------

constexpr uint8_t URL_KEY_WIDTH = 2;
constexpr uint8_t URL_WIDE_CONTROL_WIDTH = 3;

#define UK(label, output, value) \
  fui::KeyboardKey { label, output, fui::KeyKind::Normal, fui::StateNormal, value, URL_KEY_WIDTH, true, nullptr }
#define UKA(label, output, value, alt) \
  fui::KeyboardKey { label, output, fui::KeyKind::Normal, fui::StateNormal, value, URL_KEY_WIDTH, true, alt }
#define UKW(label, output, value, units) \
  fui::KeyboardKey { label, output, fui::KeyKind::Normal, fui::StateNormal, value, units *URL_KEY_WIDTH, true, nullptr }
#define UKS(label, kind, value, units) \
  fui::KeyboardKey { label, nullptr, kind, fui::StateNormal, value, units *URL_KEY_WIDTH, true, nullptr }
#define UK15(label, kind, value) \
  fui::KeyboardKey { label, nullptr, kind, fui::StateNormal, value, URL_WIDE_CONTROL_WIDTH, true, nullptr }

constexpr int16_t URL_PANEL_VALUE = -3;  // mirrors WrenKeyboardActivity::URL_PANEL_KEY

const fui::KeyboardKey URL_ROW1[] = {UK("q", "q", 'q'), UK("w", "w", 'w'), UK("e", "e", 'e'), UK("r", "r", 'r'),
                                     UK("t", "t", 't'), UK("y", "y", 'y'), UK("u", "u", 'u'), UK("i", "i", 'i'),
                                     UK("o", "o", 'o'), UK("p", "p", 'p')};
const fui::KeyboardKey URL_ROW2[] = {UK("a", "a", 'a'), UK("s", "s", 's'), UK("d", "d", 'd'),
                                     UK("f", "f", 'f'), UK("g", "g", 'g'), UK("h", "h", 'h'),
                                     UK("j", "j", 'j'), UK("k", "k", 'k'), UK("l", "l", 'l')};
const fui::KeyboardKey URL_ROW3[] = {UK15(nullptr, fui::KeyKind::Shift, fui::QWERTY_KEY_SHIFT),
                                     UK("z", "z", 'z'),
                                     UK("x", "x", 'x'),
                                     UK("c", "c", 'c'),
                                     UK("v", "v", 'v'),
                                     UK("b", "b", 'b'),
                                     UK("n", "n", 'n'),
                                     UK("m", "m", 'm'),
                                     UK15("Del", fui::KeyKind::Delete, fui::QWERTY_KEY_BACKSPACE)};
// URLs have no spaces, so the URL bottom row spends the space slot on ":",
// "/", "." and the snippet-panel toggle instead (the legacy keyboard did the
// same with its "URL" key).
const fui::KeyboardKey URL_BOTTOM[] = {UKS("?123", fui::KeyKind::Mode, fui::QWERTY_KEY_MODE, 2),
                                       UK(":", ":", ':'),
                                       UK("/", "/", '/'),
                                       UK(".", ".", '.'),
                                       UKW("URL", nullptr, URL_PANEL_VALUE, 3),
                                       UKS("OK", fui::KeyKind::Ok, fui::QWERTY_KEY_ENTER, 2)};

const fui::KeyboardKey URL_SHIFT_ROW1[] = {UK("Q", "Q", 'Q'), UK("W", "W", 'W'), UK("E", "E", 'E'), UK("R", "R", 'R'),
                                           UK("T", "T", 'T'), UK("Y", "Y", 'Y'), UK("U", "U", 'U'), UK("I", "I", 'I'),
                                           UK("O", "O", 'O'), UK("P", "P", 'P')};
const fui::KeyboardKey URL_SHIFT_ROW2[] = {UK("A", "A", 'A'), UK("S", "S", 'S'), UK("D", "D", 'D'),
                                           UK("F", "F", 'F'), UK("G", "G", 'G'), UK("H", "H", 'H'),
                                           UK("J", "J", 'J'), UK("K", "K", 'K'), UK("L", "L", 'L')};
const fui::KeyboardKey URL_SHIFT_ROW3[] = {UK15(nullptr, fui::KeyKind::Shift, fui::QWERTY_KEY_SHIFT),
                                           UK("Z", "Z", 'Z'),
                                           UK("X", "X", 'X'),
                                           UK("C", "C", 'C'),
                                           UK("V", "V", 'V'),
                                           UK("B", "B", 'B'),
                                           UK("N", "N", 'N'),
                                           UK("M", "M", 'M'),
                                           UK15("Del", fui::KeyKind::Delete, fui::QWERTY_KEY_BACKSPACE)};

// Snippet keys: multi-character outputs, stable ids above the localized-key
// range so they never collide with layout key ids.
const fui::KeyboardKey URL_SNIP_ROW1[] = {UK("https://", "https://", 2001), UK("www.", "www.", 2002),
                                          UK(".com", ".com", 2003)};
const fui::KeyboardKey URL_SNIP_ROW2[] = {UK("http://", "http://", 2004), UK("192.168.", "192.168.", 2005),
                                          UK(".org", ".org", 2006)};
const fui::KeyboardKey URL_SNIP_ROW3[] = {UK("/opds", "/opds", 2007), UK(":8080", ":8080", 2008),
                                          UK(".net", ".net", 2009)};
const fui::KeyboardKey URL_SNIP_BOTTOM[] = {UKS("abc", fui::KeyKind::Mode, fui::QWERTY_KEY_MODE, 2),
                                            UKW("URL", nullptr, URL_PANEL_VALUE, 3),
                                            UK15("Del", fui::KeyKind::Delete, fui::QWERTY_KEY_BACKSPACE),
                                            UKS("OK", fui::KeyKind::Ok, fui::QWERTY_KEY_ENTER, 2)};

#undef UK
#undef UKA
#undef UKW
#undef UKS
#undef UK15

const fui::KeyboardRow URL_ROWS[] = {{URL_ROW1, 10, 0}, {URL_ROW2, 9, 1}, {URL_ROW3, 9, 0}, {URL_BOTTOM, 6, 0}};
const fui::KeyboardRow URL_SHIFT_ROWS[] = {
    {URL_SHIFT_ROW1, 10, 0}, {URL_SHIFT_ROW2, 9, 1}, {URL_SHIFT_ROW3, 9, 0}, {URL_BOTTOM, 6, 0}};
const fui::KeyboardRow URL_SNIP_ROWS[] = {
    {URL_SNIP_ROW1, 3, 0}, {URL_SNIP_ROW2, 3, 0}, {URL_SNIP_ROW3, 3, 0}, {URL_SNIP_BOTTOM, 4, 0}};

const fui::KeyboardLayout URL_LAYOUT{URL_ROWS, 4};
const fui::KeyboardLayout URL_SHIFT_LAYOUT{URL_SHIFT_ROWS, 4};
const fui::KeyboardLayout URL_SNIPPET_LAYOUT{URL_SNIP_ROWS, 4};

}  // namespace

WrenKeyboardActivity::WrenKeyboardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string crumb,
                                           std::string title, std::string fieldLabel, std::string initialText,
                                           size_t maxLength, InputType inputType)
    : Activity("WrenKeyboard", renderer, mappedInput),
      crumb(std::move(crumb)),
      title(std::move(title)),
      fieldLabel(std::move(fieldLabel)),
      text(std::move(initialText)),
      maxLength(maxLength),
      inputType(inputType) {}

void WrenKeyboardActivity::onEnter() {
  Activity::onEnter();
  if (maxLength) text.reserve(maxLength);
  cursorPos = text.length();
  // URL layers are EN-arranged app tables; everything else opens on the UI
  // language's layout, or on an enabled one if the user switched that off.
  layoutId = inputType == InputType::Url ? fui::KeyboardLayoutId::QwertyEn : keyboard_layouts::startingLayout();
  // The key only earns its slot in the bottom row with somewhere to go.
  const uint16_t enabledLayouts = keyboard_layouts::enabled();
  showLangKey = (enabledLayouts & (enabledLayouts - 1)) != 0;
  shifted = false;
  symbols = false;
  urlPanel = false;
  cursorMode = false;
  selRow = 0;
  selCol = 0;
  delPressCount = 0;
  hintVisible = false;
  hintShowTime = 0;
  touchRouter.reset();
  touchRouter.holdMs = TOUCH_LONG_PRESS_MS;
  touchRouter.overrideHoldMs = TOUCH_DEL_LONG_PRESS_MS;
  interactionsReady = false;
  requestUpdate();
}

void WrenKeyboardActivity::onExit() { Activity::onExit(); }

const fui::KeyboardLayout& WrenKeyboardActivity::currentLayout() const {
  if (symbols) return fui::builtinKeyboardLayout(layoutId, shifted, true, false, showLangKey);
  if (inputType == InputType::Url) {
    if (urlPanel) return URL_SNIPPET_LAYOUT;
    return shifted ? URL_SHIFT_LAYOUT : URL_LAYOUT;
  }
  return fui::builtinKeyboardLayout(layoutId, shifted, false, /*numberRow=*/false, showLangKey);
}

int WrenKeyboardActivity::rowCount(int row) const {
  const auto& layout = currentLayout();
  if (row < 0 || row >= layout.rowCount) return 0;
  const bool adapt = !symbols && inputType != InputType::Url && layout.rowCount == 4 && layout.rows[2].count &&
                     layout.rows[2].keys[layout.rows[2].count - 1].value == fui::QWERTY_KEY_BACKSPACE;
  int count = layout.rows[row].count;
  if (adapt && (row == 1 || row == 3)) ++count;
  return count;
}

const fui::KeyboardKey* WrenKeyboardActivity::keyAt(int row, int col) const {
  static const fui::KeyboardKey dot{".", ".", fui::KeyKind::Normal, fui::StateNormal, '.', 2, true, nullptr};
  static const fui::KeyboardKey dash{"-", "-", fui::KeyKind::Normal, fui::StateNormal, '-', 2, true, nullptr};
  const auto& layout = currentLayout();
  if (row < 0 || row >= layout.rowCount || col < 0 || col >= rowCount(row)) return nullptr;
  const bool adapt = !symbols && inputType != InputType::Url && layout.rowCount == 4 && layout.rows[2].count &&
                     layout.rows[2].keys[layout.rows[2].count - 1].value == fui::QWERTY_KEY_BACKSPACE;
  if (adapt && row == 1 && col == layout.rows[1].count) return &layout.rows[2].keys[layout.rows[2].count - 1];
  if (adapt && row == 2 && col == layout.rows[2].count - 1) return &dot;
  if (adapt && row == 3) {
    const int ok = layout.rows[3].count - 1;
    if (col == ok) return &dash;
    if (col > ok) --col;
  }
  return &layout.rows[row].keys[col];
}

const fui::KeyboardKey* WrenKeyboardActivity::selectedKey() const { return keyAt(selRow, selCol); }

int WrenKeyboardActivity::selectedLogicalIndex() const {
  const fui::KeyboardLayout& layout = currentLayout();
  int index = 0;
  for (int r = 0; r < selRow && r < layout.rowCount; r++) {
    index += rowCount(r);
  }
  return index + selCol;
}

void WrenKeyboardActivity::clampSelection() {
  const fui::KeyboardLayout& layout = currentLayout();
  if (layout.rowCount == 0) {
    selRow = 0;
    selCol = 0;
    return;
  }
  if (selRow < 0) selRow = 0;
  if (selRow >= layout.rowCount) selRow = layout.rowCount - 1;
  const int cols = rowCount(selRow);
  if (selCol < 0) selCol = 0;
  if (selCol >= cols) selCol = cols > 0 ? cols - 1 : 0;
}

void WrenKeyboardActivity::moveSelectionRow(const int delta) {
  const fui::KeyboardLayout& layout = currentLayout();
  if (layout.rowCount == 0) return;
  const int oldCols = selRow < layout.rowCount ? rowCount(selRow) : 1;
  selRow = (selRow + delta + layout.rowCount) % layout.rowCount;
  const int newCols = rowCount(selRow);
  // Proportional column mapping keeps vertical travel intuitive between rows
  // of different key counts (e.g. a 10-key letter row over a 6-key bottom row).
  if (oldCols > 0 && newCols > 0 && oldCols != newCols) {
    selCol = wren::ui::keyboard::mapColumn(selCol, oldCols, newCols);
  }
  clampSelection();
}

void WrenKeyboardActivity::moveSelectionCol(const int delta) {
  const fui::KeyboardLayout& layout = currentLayout();
  if (selRow < 0 || selRow >= layout.rowCount) return;
  const int cols = rowCount(selRow);
  if (cols <= 0) return;
  selCol = (selCol + delta + cols) % cols;
}

bool WrenKeyboardActivity::syncSelectionToValue(const int16_t value) {
  const fui::KeyboardLayout& layout = currentLayout();
  for (int r = 0; r < layout.rowCount; r++) {
    for (int c = 0; c < rowCount(r); c++) {
      if (keyAt(r, c)->value == value) {
        selRow = r;
        selCol = c;
        return true;
      }
    }
  }
  return false;
}

size_t WrenKeyboardActivity::utf8Prev(const std::string& s, size_t pos) {
  if (pos == 0) return 0;
  pos--;
  while (pos > 0 && (static_cast<uint8_t>(s[pos]) & 0xC0) == 0x80) pos--;
  return pos;
}

size_t WrenKeyboardActivity::utf8Next(const std::string& s, size_t pos) {
  if (pos >= s.length()) return s.length();
  pos++;
  while (pos < s.length() && (static_cast<uint8_t>(s[pos]) & 0xC0) == 0x80) pos++;
  return pos;
}

void WrenKeyboardActivity::insertUtf8(const char* out) {
  RenderLock lock(*this);
  if (!out || !*out) return;
  const size_t n = strlen(out);
  if (maxLength != 0 && text.length() + n > maxLength) return;
  if (cursorPos > text.length()) cursorPos = text.length();
  text.insert(cursorPos, out, n);
  cursorPos += n;
}

bool WrenKeyboardActivity::backspaceUtf8() {
  RenderLock lock(*this);
  if (text.empty() || cursorPos == 0) return false;
  const size_t prev = utf8Prev(text, cursorPos);
  text.erase(prev, cursorPos - prev);
  cursorPos = prev;
  return true;
}

bool WrenKeyboardActivity::activateValue(const int16_t value, const bool longPress) {
  switch (value) {
    case fui::QWERTY_KEY_SHIFT: {
      RenderLock lock(*this);
      delPressCount = 0;
      hintVisible = false;
      // Letters: case toggle. Symbols: pages between "?123" and "#+=".
      shifted = !shifted;
      clampSelection();
      return true;
    }
    case fui::QWERTY_KEY_MODE: {
      RenderLock lock(*this);
      delPressCount = 0;
      hintVisible = false;
      if (urlPanel) {
        urlPanel = false;
      } else {
        symbols = !symbols;
        shifted = false;
      }
      clampSelection();
      return true;
    }
    case fui::QWERTY_KEY_LANG: {
      RenderLock lock(*this);
      delPressCount = 0;
      hintVisible = false;
      const fui::KeyboardLayoutId nextId = keyboard_layouts::next(layoutId);
      // The non-Latin tables draw the key even with one layout enabled; a
      // full-screen e-ink repaint for an unchanged keyboard costs a second.
      if (nextId == layoutId) return false;
      layoutId = nextId;
      // Shift is per-layer: carrying it across would strand the new layout in
      // upper case. Row widths differ between scripts (Cyrillic runs 12/11/11
      // against Latin's 10/9/9), so the selection has to be re-clamped.
      shifted = false;
      clampSelection();
      return true;
    }
    case URL_PANEL_KEY: {
      RenderLock lock(*this);
      delPressCount = 0;
      hintVisible = false;
      urlPanel = !urlPanel;
      symbols = false;
      shifted = false;
      clampSelection();
      return true;
    }
    case fui::QWERTY_KEY_ENTER: {
      std::string result;
      {
        RenderLock lock(*this);
        result = text;
      }
      onComplete(std::move(result));
      return false;
    }
    case fui::QWERTY_KEY_BACKSPACE:
      if (longPress) {
        RenderLock lock(*this);
        text.clear();
        cursorPos = 0;
        return true;
      }
      delPressCount++;
      if (delPressCount >= 2) {
        hintVisible = true;
        hintShowTime = millis();
      }
      backspaceUtf8();
      return true;
    default: {
      delPressCount = 0;
      hintVisible = false;
      const fui::KeyboardLayout& layer = currentLayout();
      // keyboardAltOutputFor covers explicit alts and the letter case-flip.
      const char* out = longPress ? fui::keyboardAltOutputFor(layer, value) : nullptr;
      if (!out && !symbols && inputType != InputType::Url && value == '.') out = ".";
      if (!out && !symbols && inputType != InputType::Url && value == '-') out = "-";
      if (!out) out = fui::keyboardOutputFor(layer, value);
      if (!out) return false;
      insertUtf8(out);
      if (shifted && !symbols) {
        RenderLock lock(*this);
        shifted = false;  // shift auto-releases after one character
        clampSelection();
      }
      return true;
    }
  }
}

bool WrenKeyboardActivity::clearAllOrAltOnSelected() {
  const fui::KeyboardKey* key = selectedKey();
  if (!key) return false;
  if (key->value == fui::QWERTY_KEY_BACKSPACE) {
    RenderLock lock(*this);
    text.clear();
    cursorPos = 0;
    return true;
  }
  // Explicit alts and the letter case-flip, same as touch long-press.
  const char* alt = fui::keyboardAltOutputFor(currentLayout(), key->value);
  if (alt) {
    insertUtf8(alt);
    return true;
  }
  return false;
}

fui::Rect WrenKeyboardActivity::keyboardRect() const {
  const int height = renderer.getScreenHeight();
  const int rowHeight = renderer.getScreenWidth() > height ? 40 : 44;
  const int rows = currentLayout().rowCount;
  return {20, static_cast<int16_t>(height - 56 - rows * rowHeight - (rows - 1) * 4),
          static_cast<int16_t>(renderer.getScreenWidth() - 40),
          static_cast<int16_t>(rows * rowHeight + (rows - 1) * 4)};
}

int WrenKeyboardActivity::caretAdvance(size_t position) {
  position = std::min(position, text.size());
  const bool rtl = BidiUtils::detectParagraphLevel(text.c_str()) != 0;
  const int total = rtl ? renderer.getTextAdvanceX(WREN_GOLOS_32_FONT_ID, text.c_str(), EpdFontFamily::REGULAR) : 0;
  const char saved = text[position];
  text[position] = '\0';
  const int prefix = renderer.getTextAdvanceX(WREN_GOLOS_32_FONT_ID, text.c_str(), EpdFontFamily::REGULAR);
  text[position] = saved;
  return rtl ? std::max(0, total - prefix) : prefix;
}

bool WrenKeyboardActivity::cursorPositionFromPoint(int x, int y, size_t& position) {
  if (y < 180 || y >= 240 || x < 20 || x >= renderer.getScreenWidth() - 20) return false;
  RenderLock lock(*this);
  const int target = x - 36 + fieldScroll;
  int best = 0x7fffffff;
  for (size_t i = 0;; i = utf8Next(text, i)) {
    const int distance = std::abs(target - caretAdvance(i));
    if (distance < best) {
      best = distance;
      position = i;
    }
    if (i == text.size()) break;
  }
  return true;
}

void WrenKeyboardActivity::loop() {
  int tx = 0;
  int ty = 0;

  size_t touchedCursorPos = 0;
  if (mappedInput.wasScreenTapped(tx, ty) && cursorPositionFromPoint(tx, ty, touchedCursorPos)) {
    haptic_feedback::touchAction();
    {
      RenderLock lock(*this);
      cursorPos = std::min(touchedCursorPos, text.length());
      // Keep the cursor on a UTF-8 boundary.
      while (cursorPos > 0 && cursorPos < text.length() && (static_cast<uint8_t>(text[cursorPos]) & 0xC0) == 0x80) {
        cursorPos--;
      }
    }
    cursorMode = false;
    hintVisible = false;
    touchRouter.reset();
    requestUpdate();
    return;
  }

  if (!cursorMode && interactionsReady) {
    const bool pressedDown = mappedInput.wasScreenTouchDown(tx, ty);
    int tapX = 0;
    int tapY = 0;
    const bool tapped = mappedInput.wasScreenTapped(tapX, tapY);
    int hx = 0;
    int hy = 0;
    const bool inContact = mappedInput.isScreenTouchHeld(hx, hy);

    const fui::TouchHoldRouter::Result result =
        touchRouter.update(interactions, pressedDown, static_cast<int16_t>(tx), static_cast<int16_t>(ty), tapped,
                           static_cast<int16_t>(tapX), static_cast<int16_t>(tapY), inContact, millis());
    if (result.event) {
      syncSelectionToValue(result.event.value);
      if (activateValue(result.event.value, result.event.longPress)) {
        haptic_feedback::touchAction(result.event.longPress);
        requestUpdate();
      }
      return;
    }
    if (result.activeChanged) {
      requestUpdate();
    }
    if (pressedDown || tapped) {
      return;
    }
  }

  // Front up/down move rows; side left/right move columns or the caret.
  if (!cursorMode && mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    upHeld = true;
    upLongHandled = false;
  }

  if (upHeld && !upLongHandled && mappedInput.isPressed(MappedInputManager::Button::Left) &&
      mappedInput.getHeldTime() > LONG_PRESS_MS) {
    cursorMode = true;
    upLongHandled = true;
    hintVisible = true;
    hintShowTime = millis();
    requestUpdate();
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (upHeld && !upLongHandled && !cursorMode) {
      moveSelectionRow(-1);
      requestUpdate();
    }
    upHeld = false;
    upLongHandled = false;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    downHeld = true;
    if (cursorMode) {
      cursorMode = false;
      hintVisible = false;
      downLongHandled = true;
      requestUpdate();
    } else {
      downLongHandled = false;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (downHeld && !downLongHandled && !cursorMode) {
      moveSelectionRow(1);
      requestUpdate();
    }
    downHeld = false;
    downLongHandled = false;
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] {
    if (cursorMode) return;
    moveSelectionCol(-1);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    if (cursorMode && cursorPos > 0) {
      RenderLock lock(*this);
      cursorPos = utf8Prev(text, cursorPos);
      requestUpdate();
    }
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this] {
    if (cursorMode) return;
    moveSelectionCol(1);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    if (cursorMode && cursorPos < text.length()) {
      RenderLock lock(*this);
      cursorPos = utf8Next(text, cursorPos);
      requestUpdate();
    }
    if (cursorMode) return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    confirmHeld = true;
    confirmLongHandled = false;
  }

  const fui::KeyboardKey* selKey = selectedKey();
  const bool selectedDel = selKey && selKey->value == fui::QWERTY_KEY_BACKSPACE;

  if (confirmHeld && !confirmLongHandled && mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() > DEL_LONG_PRESS_MS && selectedDel) {
    clearAllOrAltOnSelected();
    confirmLongHandled = true;
    requestUpdate();
  }

  if (confirmHeld && !confirmLongHandled && mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() > LONG_PRESS_MS) {
    if (!selectedDel && clearAllOrAltOnSelected()) {
      requestUpdate();
      confirmLongHandled = true;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (confirmHeld && !confirmLongHandled && !cursorMode) {
      if (selKey && activateValue(selKey->value, false)) {
        requestUpdate();
      }
    }
    confirmHeld = false;
    confirmLongHandled = false;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onCancel();
  }

  if (hintVisible && !cursorMode && millis() - hintShowTime > 4000) {
    hintVisible = false;
    requestUpdate();
  }
}

void WrenKeyboardActivity::render(RenderLock&&) {
  using namespace wren::ui;
  wren::setup::status(renderer);
  drawHeader(renderer, 28, crumb.c_str(), title.c_str());
  const int width = renderer.getScreenWidth();
  drawLabel(renderer, 20, 152, fieldLabel.c_str());
  constexpr int fieldY = 180;
  renderer.drawRect(20, fieldY, width - 40, 60, 3, true);
  const int viewport = width - 72;
  const int caret = caretAdvance(cursorPos);
  fieldScroll = keyboard::scrollOffset(fieldScroll, caret, 3, viewport);
  renderer.setClipRect(36, fieldY + 3, viewport, 54);
  renderer.drawText(WREN_GOLOS_32_FONT_ID, 36 - fieldScroll, fieldY + 10, text.c_str());
  renderer.fillRect(36 + caret - fieldScroll, fieldY + 14, 3, 32, true);
  wren::setup::unclip(renderer);

  interactions.beginPublishCycle();
  interactions.clear();
  const auto& layout = currentLayout();
  const auto bounds = keyboardRect();
  const int rowHeight = width > renderer.getScreenHeight() ? 40 : 44;
  for (int r = 0; r < layout.rowCount; ++r) {
    const int cols = rowCount(r);
    int units = 0, count = 0;
    for (int c = 0; c < cols; ++c) {
      const auto& key = *keyAt(r, c);
      if (!showLangKey && key.value == fui::QWERTY_KEY_LANG) continue;
      units += key.value == fui::QWERTY_KEY_SHIFT && !symbols       ? 4
               : key.value == fui::QWERTY_KEY_BACKSPACE && !symbols ? 2
               : key.value == fui::QWERTY_KEY_SPACE && !symbols     ? (showLangKey ? 8 : 10)
                                                                    : key.widthUnits;
      ++count;
    }
    if (!units || !count) continue;
    int used = 0, visible = 0;
    for (int c = 0; c < cols; ++c) {
      const auto& key = *keyAt(r, c);
      if (!showLangKey && key.value == fui::QWERTY_KEY_LANG) continue;
      const int usable = bounds.width - 4 * (count - 1);
      const int left = bounds.x + usable * used / units + 4 * visible;
      const int weight = key.value == fui::QWERTY_KEY_SHIFT && !symbols       ? 4
                         : key.value == fui::QWERTY_KEY_BACKSPACE && !symbols ? 2
                         : key.value == fui::QWERTY_KEY_SPACE && !symbols     ? (showLangKey ? 8 : 10)
                                                                              : key.widthUnits;
      const int keyWidth = usable * (used + weight) / units - usable * used / units;
      const int top = bounds.y + r * (rowHeight + 4);
      used += weight;
      ++visible;
      const bool selected = !cursorMode && selRow == r && selCol == c;
      const bool solid = selected || key.value == fui::QWERTY_KEY_ENTER;
      renderer.fillRect(left, top, keyWidth, rowHeight, solid);
      renderer.drawRect(left, top, keyWidth, rowHeight, 2, true);
      if (selected && key.value == fui::QWERTY_KEY_ENTER)
        renderer.drawRect(left + 4, top + 4, keyWidth - 8, rowHeight - 8, 3, false);
      const char* label = key.label ? key.label : "";
      bool function = true;
      switch (key.value) {
        case fui::QWERTY_KEY_SHIFT:
          label = tr(STR_WREN_KB_SHIFT);
          break;
        case fui::QWERTY_KEY_BACKSPACE:
          label = tr(STR_WREN_KB_DEL);
          break;
        case fui::QWERTY_KEY_MODE:
          label = symbols || urlPanel ? tr(STR_WREN_KB_ABC) : "123";
          break;
        case fui::QWERTY_KEY_SPACE:
          label = tr(STR_WREN_KB_SPACE);
          break;
        case fui::QWERTY_KEY_ENTER:
          label = tr(STR_WREN_KB_OK);
          break;
        case fui::QWERTY_KEY_LANG:
          label = keyboard::languageLabel(languageKey(keyboard_layouts::next(layoutId)), showLangKey);
          break;
        default:
          function = false;
          break;
      }
      if (function) {
        renderer.setClipRect(left + 2, top + 2, keyWidth - 4, rowHeight - 4);
        const int labelWidth = wren::ui::labelWidth(renderer, label);
        drawLabel(renderer, left + std::max(2, (keyWidth - labelWidth) / 2), top + (rowHeight - 16) / 2, label, !solid);
        wren::setup::unclip(renderer);
      } else {
        const int textWidth = wren::ui::textWidth(renderer, WREN_GOLOS_24_FONT_ID, label);
        renderer.setClipRect(left + 2, top + 2, keyWidth - 4, rowHeight - 4);
        renderer.drawText(WREN_GOLOS_24_FONT_ID, left + std::max(2, (keyWidth - textWidth) / 2),
                          top + (rowHeight - 24) / 2, label, !solid);
        wren::setup::unclip(renderer);
      }
      interactions.addInteraction({{static_cast<int16_t>(left), static_cast<int16_t>(top),
                                    static_cast<int16_t>(keyWidth), static_cast<int16_t>(rowHeight)},
                                   ACTION_KEY,
                                   key.value,
                                   static_cast<uint16_t>(fui::InputTouch | fui::InputLongPress),
                                   fui::StateNormal});
    }
  }
  interactions.publish();
  interactionsReady = true;
  wren::setup::footer(renderer);
  renderer.displayBuffer();
}

void WrenKeyboardActivity::onComplete(std::string text) {
  setResult(KeyboardResult{std::move(text)});
  finish();
}

void WrenKeyboardActivity::onCancel() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}
