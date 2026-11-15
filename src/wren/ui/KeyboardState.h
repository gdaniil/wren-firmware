#pragma once

namespace wren::ui::keyboard {

// Divide the available width without losing pixels to integer rounding.
constexpr int keyLeft(int width, int count, int gap, int index) {
  return count > 0 ? ((width - gap * (count - 1)) * index / count) + gap * index : 0;
}
constexpr int keyWidth(int width, int count, int gap, int index) {
  return count > 0 ? (width - gap * (count - 1)) * (index + 1) / count - (width - gap * (count - 1)) * index / count
                   : 0;
}
constexpr int mapColumn(int column, int oldCount, int newCount) {
  return oldCount > 0 && newCount > 0 ? column * newCount / oldCount : 0;
}

enum class LanguageKey { En, Fr, De, Es, Ru, Uk, Be, Kk, He, Ar, El };
constexpr const char* languageLabel(LanguageKey language, bool multiple) {
  if (!multiple) return "";
  switch (language) {
    case LanguageKey::En:
      return "en";
    case LanguageKey::Fr:
      return "fr";
    case LanguageKey::De:
      return "de";
    case LanguageKey::Es:
      return "es";
    case LanguageKey::Ru:
      return "ru";
    case LanguageKey::Uk:
      return "ua";
    case LanguageKey::Be:
      return "be";
    case LanguageKey::Kk:
      return "kk";
    case LanguageKey::He:
      return "he";
    case LanguageKey::Ar:
      return "ar";
    case LanguageKey::El:
      return "el";
  }
  return "";
}

enum class Focus { Keys, PasswordSwitch };
struct Selection {
  Focus focus = Focus::Keys;
  int row = 0;
  int column = 0;

  void moveRow(int delta, int rowCount, const int* counts, bool password) {
    if (rowCount <= 0) return;
    if (focus == Focus::PasswordSwitch) {
      if (delta > 0) focus = Focus::Keys;
      return;
    }
    if (password && row == 0 && delta < 0) {
      focus = Focus::PasswordSwitch;
      return;
    }
    const int next = (row + delta + rowCount) % rowCount;
    column = mapColumn(column, counts[row], counts[next]);
    row = next;
  }
};

// Pixel offset of the viewport, with a caret of the given width kept inside it.
constexpr int scrollOffset(int offset, int caret, int caretWidth, int viewportWidth) {
  if (caret < offset) offset = caret;
  if (caret + caretWidth > offset + viewportWidth) offset = caret + caretWidth - viewportWidth;
  return offset < 0 ? 0 : offset;
}

}  // namespace wren::ui::keyboard
