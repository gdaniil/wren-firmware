#include "WrenText.h"

#include <cstddef>

namespace wren {
namespace {
char lowerAscii(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; }

bool matchesLanguage(const char* language, const char* two, const char* three) {
  size_t i = 0;
  while (language[i] && language[i] != '-' && language[i] != '_') ++i;
  if (i != 2 && i != 3) return false;
  const char* expected = i == 2 ? two : three;
  for (size_t j = 0; j < i; ++j) {
    if (lowerAscii(language[j]) != expected[j]) return false;
  }
  return true;
}
}  // namespace

TextLanguage textLanguage(const char* language) {
  if (!language) return TextLanguage::None;
  if (matchesLanguage(language, "ru", "rus")) return TextLanguage::Russian;
  if (matchesLanguage(language, "uk", "ukr")) return TextLanguage::Ukrainian;
  if (matchesLanguage(language, "en", "eng")) return TextLanguage::English;
  return TextLanguage::None;
}
}  // namespace wren

namespace wren {
namespace {
uint32_t next(std::string_view text, size_t& i) {
  const uint8_t c = text[i++];
  if (c < 128) return c;
  const unsigned count = c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
  uint32_t cp = c & (count == 1 ? 31 : count == 2 ? 15 : 7);
  for (unsigned n = 0; n < count && i < text.size(); ++n) cp = (cp << 6) | (text[i++] & 63);
  return cp;
}
bool letter(uint32_t cp) {
  return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || (cp >= 0x400 && cp <= 0x52f);
}
bool digit(uint32_t cp) { return cp >= '0' && cp <= '9'; }
bool opening(uint32_t cp) { return cp == '(' || cp == '[' || cp == '{' || cp == '-' || cp == 0x2014 || cp == 0x2013; }
bool closingPunctuation(uint32_t cp) {
  return cp == 0 || cp == '"' || cp == ',' || cp == '.' || cp == ';' || cp == ':' || cp == '!' || cp == '?' ||
         cp == ')' || cp == ']' || cp == '}';
}
bool cyrillic(TextLanguage lang) { return lang == TextLanguage::Russian || lang == TextLanguage::Ukrainian; }
bool listed(std::string_view word, std::string_view list) {
  while (!list.empty()) {
    const size_t end = list.find(' ');
    if (word == list.substr(0, end)) return true;
    if (end == std::string_view::npos) break;
    list.remove_prefix(end + 1);
  }
  return false;
}
std::string_view bare(std::string_view word) {
  while (!word.empty() &&
         (word.back() == ',' || word.back() == ';' || word.back() == ':' || word.back() == '!' || word.back() == '?'))
    word.remove_suffix(1);
  return word;
}
bool number(std::string_view word) {
  if (word.empty()) return false;
  bool seen = false;
  for (char c : word) {
    if (c >= '0' && c <= '9')
      seen = true;
    else if (c != '.' && c != ',' && c != '+' && c != '-')
      return false;
  }
  return seen;
}
}  // namespace
uint32_t firstCharacter(std::string_view text) {
  size_t i = 0;
  return text.empty() ? 0 : next(text, i);
}
uint32_t lastCharacter(std::string_view text) {
  size_t i = 0;
  uint32_t cp = 0;
  while (i < text.size()) cp = next(text, i);
  return cp;
}
size_t rewriteText(std::string_view in, char* out, TextLanguage lang, QuoteState& state, uint32_t previous,
                   uint32_t following, void (*absorbed)(uint32_t, void*), void* context, uint8_t* focusBoundary) {
  size_t written = 0, i = 0;
  uint32_t index = 0;
  const uint8_t oldFocus = focusBoundary ? *focusBoundary : 0;
  size_t newFocus = 0;
  while (i < in.size()) {
    const size_t start = i;
    const uint32_t cp = next(in, i);
    size_t peek = i;
    const uint32_t after = i < in.size() ? next(in, peek) : following;
    const char* replacement = nullptr;
    unsigned removed = 0;
    if (lang != TextLanguage::None) {
      if (cp == '.' && in.substr(start, 3) == "...") {
        replacement = "…";
        i = start + 3;
        removed = 2;
      } else if (lang == TextLanguage::English && cp == '-' && in.substr(start, 2) == "--") {
        replacement = "—";
        i = start + 2;
        removed = 1;
      } else if (cyrillic(lang) && (in == "-" || in == "–"))
        replacement = "—";
      else if (cp == '"') {
        const bool open = (previous == 0 || opening(previous)) && !closingPunctuation(after);
        if (open) {
          replacement = lang == TextLanguage::English ? "“" : state.doubleDepth == 0 ? "«" : "„";
          if (state.doubleDepth != UINT16_MAX) ++state.doubleDepth;
        } else {
          replacement = lang == TextLanguage::English ? "”" : state.doubleDepth > 1 ? "“" : "»";
          if (state.doubleDepth) --state.doubleDepth;
        }
      } else if (cp == '\'') {
        if (lang == TextLanguage::English) {
          if (letter(previous) || digit(previous) || digit(after)) {
            replacement = "’";
            if (state.singleOpen && closingPunctuation(after)) state.singleOpen = false;
          } else {
            replacement = state.singleOpen ? "’" : "‘";
            state.singleOpen = !state.singleOpen;
          }
        } else if (letter(previous) && letter(after))
          replacement = "’";
      }
    }
    if (replacement) {
      for (const char* p = replacement; *p; ++p) out[written++] = *p;
    } else {
      for (size_t p = start; p < i; ++p) out[written++] = in[p];
    }
    if (oldFocus && start < oldFocus) newFocus = written;
    for (unsigned n = 1; n <= removed; ++n)
      if (absorbed) absorbed(index + n, context);
    index += 1 + removed;
    previous = replacement ? lastCharacter(replacement) : cp;
  }
  out[written] = '\0';
  if (focusBoundary) *focusBoundary = static_cast<uint8_t>(newFocus < 256 ? newFocus : 0);
  return written;
}
bool isGlueAfter(std::string_view word, TextLanguage lang) {
  if (lang == TextLanguage::English) return listed(word, "Mr. Mrs. Ms. Dr. St.");
  if (!cyrillic(lang)) return false;
  if (word == "№") return true;
  size_t i = 0;
  if (word.empty()) return false;
  const uint32_t cp = next(word, i);
  return word.substr(i) == "." && ((cp >= 'A' && cp <= 'Z') || (cp >= 0x410 && cp <= 0x42f) || cp == 0x401 ||
                                   cp == 0x404 || cp == 0x406 || cp == 0x407 || cp == 0x490);
}
bool isGlueBefore(std::string_view word, TextLanguage lang) {
  word = bare(word);
  if (lang == TextLanguage::English) return listed(word, "kg g mg km m cm mm l ml s min h % °C °F lb oz ft in mi");
  return cyrillic(lang) && listed(word, "кг г мг т км м см мм л мл с мин ч % °C руб грн коп тыс млн млрд стр гл");
}
bool gluePair(std::string_view left, std::string_view right, TextLanguage lang, bool dialogueDash) {
  if (lang == TextLanguage::None) return false;
  if (isGlueBefore(right, lang) && number(left)) return true;
  if (isGlueAfter(left, lang) && (left != "№" || number(bare(right)))) return true;
  if (!cyrillic(lang)) return false;
  return right == "—" || (dialogueDash && left == "—") || (left == "т." && listed(bare(right), "е. д. п.")) ||
         (left == "и" && right == "т.");
}
bool isShortLineEndWord(std::string_view word, TextLanguage lang) {
  if (!cyrillic(lang) || word.empty() || word.size() > 4) return false;
  // Lowercase the first Cyrillic capital so sentence-initial "В" matches "в".
  char lowered[5] = {};
  word.copy(lowered, word.size());
  const auto b0 = static_cast<uint8_t>(lowered[0]), b1 = static_cast<uint8_t>(lowered[1]);
  if (b0 == 0xd0 && b1 >= 0x90 && b1 <= 0x9f) {
    lowered[1] = static_cast<char>(b1 + 0x20);  // А..П -> а..п
  } else if (b0 == 0xd0 && b1 >= 0xa0 && b1 <= 0xaf) {
    lowered[0] = static_cast<char>(0xd1);  // Р..Я -> р..я
    lowered[1] = static_cast<char>(b1 - 0x20);
  } else if (b0 == 0xd0 && (b1 == 0x84 || b1 == 0x86)) {
    lowered[0] = static_cast<char>(0xd1);  // Є І -> є і
    lowered[1] = static_cast<char>(b1 + 0x10);
  }
  const std::string_view lower(lowered, word.size());
  // Prepositions, conjunctions and particles only (spec): pronouns like "он" may end a line.
  if (lang == TextLanguage::Ukrainian) return listed(lower, "в у з і й а та на по за із зі від до не ні що як чи");
  return listed(lower, "в к с о у и а но на по за из от до не ни же ли бы об во ко со");
}
long long shortWordPenalty(std::string_view word, TextLanguage lang, uint16_t emHundredths) {
  if (!isShortLineEndWord(word, lang)) return 0;
  return (4LL * emHundredths * emHundredths + 5000) / 10000;
}
}  // namespace wren
