#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace wren {
enum class TextLanguage : uint8_t { None, Russian, Ukrainian, English };
TextLanguage textLanguage(const char* language);
struct QuoteState {
  uint16_t doubleDepth = 0;
  bool singleOpen = false;
};
// Output needs at most 3 * input.size() + 1 bytes. No allocation; callback indexes
// absorbed input codepoints (the first codepoint of a replacement survives).
size_t rewriteText(std::string_view input, char* output, TextLanguage language, QuoteState& state,
                   uint32_t previous = 0, uint32_t following = 0, void (*absorbed)(uint32_t, void*) = nullptr,
                   void* context = nullptr, uint8_t* focusBoundary = nullptr);
bool isGlueAfter(std::string_view word, TextLanguage language);
bool isGlueBefore(std::string_view word, TextLanguage language);
bool gluePair(std::string_view left, std::string_view right, TextLanguage language, bool dialogueDash = false);
bool isShortLineEndWord(std::string_view word, TextLanguage language);
long long shortWordPenalty(std::string_view word, TextLanguage language, uint16_t emHundredths);
uint32_t firstCharacter(std::string_view text);
uint32_t lastCharacter(std::string_view text);
}  // namespace wren
