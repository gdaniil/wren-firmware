#include <Epub/blocks/TextBlock.h>
#include <GfxRenderer.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#define class struct
#define private public
#include <Epub/ParsedText.h>
#undef private
#undef class
#include <Epub/WrenText.h>
#include <Epub/hyphenation/Hyphenator.h>

namespace {
using wren::TextLanguage;
std::string rewrite(const char* input, TextLanguage lang) {
  wren::QuoteState state;
  std::string output;
  std::string_view text(input);
  while (!text.empty()) {
    const auto end = text.find(' ');
    const auto word = text.substr(0, end);
    char buffer[1024];
    const auto length = wren::rewriteText(word, buffer, lang, state);
    output.append(buffer, length);
    if (end == std::string_view::npos) break;
    output += ' ';
    text.remove_prefix(end + 1);
  }
  return output;
}
ReaderRenderSpec spec() {
  ReaderRenderSpec result;
  result.linePitchPx = 20;
  result.fontEmHundredths = 1600;
  return result;
}
void add(ParsedText& text, std::initializer_list<const char*> words) {
  uint32_t offset = 0;
  for (const auto* word : words) {
    text.addWord(word, EpdFontFamily::REGULAR, false, false, offset);
    offset += static_cast<uint32_t>(
                  std::count_if(word, word + strlen(word), [](unsigned char c) { return (c & 0xc0) != 0x80; })) +
              1;
  }
}
}  // namespace
TEST(WrenText, LanguageTags) {
  for (const auto* tag : {"ru", "ru-RU", "RUS", "rus_RU"}) EXPECT_EQ(wren::textLanguage(tag), TextLanguage::Russian);
  for (const auto* tag : {"uk", "uk-UA", "UKR"}) EXPECT_EQ(wren::textLanguage(tag), TextLanguage::Ukrainian);
  for (const auto* tag : {"en", "en-GB", "ENG"}) EXPECT_EQ(wren::textLanguage(tag), TextLanguage::English);
  for (const auto* tag : {"", "de", "r", "rubbish"}) EXPECT_EQ(wren::textLanguage(tag), TextLanguage::None);
  EXPECT_EQ(wren::textLanguage(nullptr), TextLanguage::None);
}
TEST(WrenText, RussianQuotesDashesAndEllipsis) {
  EXPECT_EQ(rewrite("\"Он сказал: \"привет\"\"", TextLanguage::Russian), "«Он сказал: „привет“»");
  EXPECT_EQ(rewrite("слово - слово ...", TextLanguage::Russian), "слово — слово …");
  EXPECT_EQ(rewrite("- Да, - ответил он.", TextLanguage::Russian), "— Да, — ответил он.");
  EXPECT_EQ(rewrite("слово – слово", TextLanguage::Russian), "слово — слово");
}
TEST(WrenText, UkrainianApostropheAndQuotes) { EXPECT_EQ(rewrite("\"м'ята\"", TextLanguage::Ukrainian), "«м’ята»"); }
TEST(WrenText, EnglishQuotesApostrophesAndDashes) {
  EXPECT_EQ(rewrite("\"It's the dogs' '90s,\" she said--quietly.", TextLanguage::English),
            "“It’s the dogs’ ’90s,” she said—quietly.");
  EXPECT_EQ(rewrite("'hello' ... a—b", TextLanguage::English), "‘hello’ … a—b");
}
TEST(WrenText, GluePredicates) {
  for (auto lang : {TextLanguage::Russian, TextLanguage::Ukrainian}) {
    for (const auto& pair : {std::pair{"№", "5"},
                             {"5", "кг"},
                             {"А.", "С."},
                             {"С.", "Пушкин"},
                             {"т.", "е."},
                             {"т.", "д."},
                             {"т.", "п."},
                             {"и", "т."},
                             {"слово", "—"}})
      EXPECT_TRUE(wren::gluePair(pair.first, pair.second, lang));
    EXPECT_TRUE(wren::gluePair("—", "Да,", lang, true));
    EXPECT_FALSE(wren::gluePair("—", "слово", lang));
    EXPECT_FALSE(wren::gluePair("слово", "кг", lang));
  }
  EXPECT_TRUE(wren::gluePair("5", "kg", TextLanguage::English));
  EXPECT_TRUE(wren::gluePair("Dr.", "Watson", TextLanguage::English));
  EXPECT_FALSE(wren::gluePair("word", "—", TextLanguage::English));
}
TEST(WrenText, GatesPreserveBytes) {
  for (bool off : {false, true}) {
    ParsedText text;
    text.setWrenTypography(off ? ReaderRenderSpec{} : spec(), off ? TextLanguage::Russian : TextLanguage::None);
    add(text, {"\"word\"", "...", "--"});
    ASSERT_TRUE(text.preprocessWrenText());
    EXPECT_EQ(text.wordAt(0), "\"word\"");
    EXPECT_EQ(text.wordAt(1), "...");
    EXPECT_EQ(text.wordAt(2), "--");
  }
}
TEST(WrenText, SpaceTokenKeepsGlueAndNormalWidth) {
  for (auto alignment : {CssTextAlign::Left, CssTextAlign::Justify}) {
    BlockStyle style;
    style.alignment = alignment;
    ParsedText text(false, false, style, 0);
    text.setWrenTypography(spec(), TextLanguage::English);
    add(text, {"5", "kg", "end"});
    GfxRenderer renderer;
    size_t lines = 0;
    text.layoutAndExtractLines(renderer, 0, 28, [&](std::unique_ptr<TextBlock> block, auto) {
      if (lines++ == 0) {
        ASSERT_EQ(block->wordCount(), 3);
        EXPECT_STREQ(block->wordText(0), "5");
        EXPECT_STREQ(block->wordText(1), " ");
        EXPECT_STREQ(block->wordText(2), "kg");
        EXPECT_EQ(block->wordXpos(2), 12);
      }
    });
    EXPECT_EQ(lines, 2u);
  }
}
TEST(WrenText, SourceOffsetsIncludeEllipsisDashAndNfcAbsorption) {
  ParsedText text;
  text.setWrenTypography(spec(), TextLanguage::English);
  // NFC absorption already exists later in this word when preprocessing starts.
  add(text, {"a...be\xcc\x81--c", "next"});
  ASSERT_TRUE(text.preprocessWrenText());
  EXPECT_EQ(text.wordAt(0), "a…bé—");  // explicit dash tokenization occurs before preprocessing
  EXPECT_EQ(text.sourceOffsetAfter(0, 2), 4u);
  EXPECT_EQ(text.sourceOffsetAfter(0, 4), 7u);
  EXPECT_EQ(text.sourceOffsetAfter(0, 5), 9u);
  EXPECT_EQ(text.visibleOffsetAt(text.size() - 1), 11u);
  GfxRenderer renderer;
  // A real soft-hyphen split after a contracted ellipsis keeps the source coordinate.
  ParsedText split(true, false, BlockStyle{}, 0);
  split.setWrenTypography(spec(), TextLanguage::English);
  add(split, {"ab...cd\xc2\xad"
              "efgh"});
  ASSERT_TRUE(split.preprocessWrenText());
  Hyphenator::setPreferredLanguage("en");
  auto splitWidths = split.calculateWordWidths(renderer, 0);
  ASSERT_TRUE(split.hyphenateWordAtIndex(0, 48, renderer, 0, splitWidths, false));
  EXPECT_EQ(split.visibleOffsetAt(1), 8u);
}
TEST(WrenText, InlineQuotesAndParagraphReset) {
  ParsedText text;
  text.setWrenTypography(spec(), TextLanguage::Russian);
  text.addWord("\"слово", EpdFontFamily::ITALIC);
  text.addWord("\"", EpdFontFamily::REGULAR, false, true, 6);
  ASSERT_TRUE(text.preprocessWrenText());
  EXPECT_EQ(text.wordAt(0), "«слово");
  EXPECT_EQ(text.wordAt(1), "»");
  EXPECT_EQ(rewrite("\"новый", TextLanguage::Russian), "«новый");
}
TEST(WrenText, ShortWordPenaltyAndGreedyPreference) {
  EXPECT_EQ(wren::shortWordPenalty("в", TextLanguage::Russian, 1600), 1024);
  EXPECT_EQ(wren::shortWordPenalty("word", TextLanguage::English, 1600), 0);
  for (bool greedy : {false, true}) {
    ParsedText text(false, false, BlockStyle{}, 0);
    auto config = spec();
    config.hyphenationZone = greedy ? 75 : 0;
    text.setWrenTypography(config, TextLanguage::Russian);
    add(text, {"слово", "в", "доме"});
    GfxRenderer renderer;
    std::vector<std::string> lines;
    text.layoutAndExtractLines(renderer, 0, 56, [&](std::unique_ptr<TextBlock> block, auto) {
      lines.emplace_back(block->wordText(block->wordCount() - 1));
    });
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "слово");
    EXPECT_EQ(lines[1], "доме");
  }
  ParsedText last;
  last.setWrenTypography(spec(), TextLanguage::Russian);
  add(last, {"слово", "в"});
  GfxRenderer renderer;
  size_t lines = 0;
  last.layoutAndExtractLines(renderer, 0, 56, [&](auto, auto) { ++lines; });
  EXPECT_EQ(lines, 1u);
}

TEST(WrenText, ParagraphGlueUsesUnbreakableVisibleSpaces) {
  for (auto lang : {TextLanguage::Russian, TextLanguage::Ukrainian}) {
    ParsedText text;
    text.setWrenTypography(spec(), lang);
    add(text, {"-", "Да,", "слово", "-", "слово", "№", "5", "кг", "А.", "С.", "Пушкин", "т.", "е.", "и", "т.", "д."});
    ASSERT_TRUE(text.preprocessWrenText());
    const auto linked = [&](std::string_view left, std::string_view right) {
      for (size_t i = 1; i + 1 < text.size(); ++i) {
        if (text.wordAt(i - 1) == left && text.wordAt(i) == " " && text.wordAt(i + 1) == right)
          return text.wordContinues[i] && text.wordContinues[i + 1] && !text.wordNoSpaceBefore[i + 1];
      }
      return false;
    };
    EXPECT_TRUE(linked("—", "Да,"));
    EXPECT_TRUE(linked("слово", "—"));
    EXPECT_TRUE(linked("№", "5"));
    EXPECT_TRUE(linked("5", "кг"));
    EXPECT_TRUE(linked("А.", "С."));
    EXPECT_TRUE(linked("С.", "Пушкин"));
    EXPECT_TRUE(linked("т.", "е."));
    EXPECT_TRUE(linked("и", "т."));
    EXPECT_TRUE(linked("т.", "д."));
    const auto count = text.size();
    ASSERT_TRUE(text.preprocessWrenText());
    EXPECT_EQ(text.size(), count);
  }
}
TEST(WrenText, EveryFixedUnitGluesOnlyAfterNumber) {
  for (auto lang : {TextLanguage::Russian, TextLanguage::English}) {
    std::string_view list = lang == TextLanguage::Russian
                                ? "кг г мг т км м см мм л мл с мин ч % °C руб грн коп тыс млн млрд стр гл"
                                : "kg g mg km m cm mm l ml s min h % °C °F lb oz ft in mi";
    while (!list.empty()) {
      const auto end = list.find(' ');
      const auto unit = list.substr(0, end);
      EXPECT_TRUE(wren::gluePair("5", unit, lang));
      EXPECT_FALSE(wren::gluePair("word", unit, lang));
      if (end == std::string_view::npos) break;
      list.remove_prefix(end + 1);
    }
  }
}
TEST(WrenText, SoftFlushDoesNotRewriteOrAbsorbTwice) {
  ParsedText text;
  text.setWrenTypography(spec(), TextLanguage::English);
  add(text, {"one...", "two", "three", "four"});
  GfxRenderer renderer;
  std::vector<TextBlock::SourceRange> ranges;
  const auto emit = [&](std::unique_ptr<TextBlock> line, auto) {
    for (uint16_t i = 0; i < line->wordCount(); ++i) ranges.push_back(line->wordSourceRange(i));
  };
  text.layoutAndExtractLines(renderer, 0, 56, emit, false);
  text.addWord("five...", EpdFontFamily::REGULAR, false, false, 25);
  text.layoutAndExtractLines(renderer, 0, 56, emit);
  ASSERT_FALSE(ranges.empty());
  EXPECT_EQ(ranges.front().start, 0u);
  EXPECT_EQ(ranges.front().end, 6u);
  EXPECT_EQ(ranges.back().start, 25u);
  EXPECT_EQ(ranges.back().end, 32u);
}

TEST(WrenText, RepeatedEnglishSingleQuotesReset) {
  EXPECT_EQ(rewrite("'one' 'two' dogs' '90s", TextLanguage::English), "‘one’ ‘two’ dogs’ ’90s");
}
TEST(WrenText, AbsorbedOffsetsStaySortedAcrossChunkSplits) {
  ParsedText text;
  text.setWrenTypography(spec(), TextLanguage::English);
  std::string word;
  for (int i = 0; i < 40; ++i) word += "a...e\xcc\x81";
  text.addWord(word, EpdFontFamily::REGULAR);
  ASSERT_TRUE(text.preprocessWrenText());
  for (uint32_t i = 1; i <= 40; ++i) EXPECT_EQ(text.sourceOffsetAfter(0, i * 3), i * 6);
}
TEST(WrenText, GreedyLastLineAndOversizedGlueStillAdvance) {
  GfxRenderer renderer;
  for (bool oversized : {false, true}) {
    ParsedText text(false, false, BlockStyle{}, 0);
    auto config = spec();
    config.hyphenationZone = 75;
    text.setWrenTypography(config, oversized ? TextLanguage::English : TextLanguage::Russian);
    if (oversized)
      add(text, {"Dr.", "Watson"});
    else
      add(text, {"слово", "в"});
    size_t lines = 0;
    text.layoutAndExtractLines(renderer, 0, oversized ? 24 : 56, [&](auto, auto) { ++lines; });
    EXPECT_EQ(lines, 1u);
    EXPECT_TRUE(text.isEmpty());
  }
}

TEST(WrenText, ShortWordListIsPrepositionsOnly) {
  EXPECT_TRUE(wren::isShortLineEndWord("в", TextLanguage::Russian));
  EXPECT_TRUE(wren::isShortLineEndWord("В", TextLanguage::Russian));
  EXPECT_TRUE(wren::isShortLineEndWord("На", TextLanguage::Russian));
  EXPECT_TRUE(wren::isShortLineEndWord("Із", TextLanguage::Ukrainian));
  EXPECT_FALSE(wren::isShortLineEndWord("он", TextLanguage::Russian));
  EXPECT_FALSE(wren::isShortLineEndWord("мы", TextLanguage::Russian));
  EXPECT_FALSE(wren::isShortLineEndWord("и,", TextLanguage::Russian));
  EXPECT_FALSE(wren::isShortLineEndWord("to", TextLanguage::English));
}
