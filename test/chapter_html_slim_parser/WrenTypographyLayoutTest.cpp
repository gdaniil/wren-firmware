#include <Epub.h>
#include <Epub/Page.h>
#include <Epub/hyphenation/Hyphenator.h>
#include <GfxRenderer.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>
#define class struct
#define private public
#include "Epub/parsers/ChapterHtmlSlimParser.h"
#undef private
#undef class

namespace {
struct LayoutLine {
  int y;
  int x;
  std::string text;
};
using LayoutPage = std::vector<LayoutLine>;
std::vector<LayoutPage> layout(const char* body, ReaderRenderSpec spec, int width = 100, int height = 85,
                               bool twoParagraphs = false, const char* language = "en", int bodyGlyphHeight = 16) {
  std::string filepath = "unused.xhtml";
  GfxRenderer renderer;
  renderer.fontMap.emplace(0, EpdFontFamily(12, nullptr));
  renderer.fontLineHeights[0] = bodyGlyphHeight;
  std::vector<LayoutPage> pages;
  ChapterHtmlSlimParser parser(
      nullptr, filepath, renderer, 0, 1.0f, false, 1, width, height, true, false,
      [&](std::unique_ptr<Page> page, auto, auto, auto) {
        auto& result = pages.emplace_back();
        for (auto& element : page->elements) {
          if (element->getTag() != TAG_PageLine) continue;
          const auto* block = static_cast<PageLine&>(*element).getBlock();
          LayoutLine line{element->yPos, block->wordXpos(0), {}};
          for (unsigned i = 0; i < block->wordCount(); ++i) {
            if (i) line.text += ' ';
            line.text += block->wordText(i);
          }
          result.push_back(line);
        }
      },
      true, "", "", 0, {}, nullptr, nullptr);
  parser.setWrenTypography(spec, language);
  Hyphenator::setPreferredLanguage(language);
  parser.beginParse();
  const std::string paragraph = "<p>" + std::string(body) + "</p>";
  const std::string html = "<html><body>" + paragraph + (twoParagraphs ? paragraph : "") + "</body></html>";
  XML_Parser xml = XML_ParserCreate(nullptr);
  XML_SetUserData(xml, &parser);
  XML_SetElementHandler(xml, ChapterHtmlSlimParser::startElement, ChapterHtmlSlimParser::endElement);
  XML_SetCharacterDataHandler(xml, ChapterHtmlSlimParser::characterData);
  EXPECT_EQ(XML_Parse(xml, html.data(), static_cast<int>(html.size()), true), XML_STATUS_OK);
  XML_ParserFree(xml);
  parser.completeCurrentPage();
  return pages;
}
ReaderRenderSpec wrenSpec() {
  ReaderRenderSpec spec;
  spec.linePitchPx = 20;
  spec.hyphenationZone = 75;
  spec.hyphenationMinWordLetters = 8;
  spec.hyphenationMinLeft = 2;
  spec.hyphenationMinRight = 3;
  spec.hyphenationMaxConsecutive = 2;
  return spec;
}
constexpr const char* WORDS = "aa abcdef\xC2\xADghijkl aa abcdef\xC2\xADghijkl aa abcdef\xC2\xADghijkl aa";
bool hyphenated(const LayoutLine& line) { return !line.text.empty() && line.text.back() == '-'; }
}  // namespace
TEST(WrenLayout, ConstantPitchAndTopRemainder) {
  const auto pages = layout("one two three four five six seven eight nine ten eleven", wrenSpec());
  ASSERT_FALSE(pages.empty());
  ASSERT_GE(pages[0].size(), 2u);
  EXPECT_EQ(pages[0][0].y, 5);
  for (const auto& page : pages)
    for (size_t i = 1; i < page.size(); ++i) EXPECT_EQ(page[i].y - page[i - 1].y, 20);
}
TEST(WrenLayout, EmIndentUsesFontEm) {
  auto spec = wrenSpec();
  spec.paragraphIndentEm = 100;
  spec.fontEmHundredths = 2800;
  const auto pages = layout("one two three four", spec);
  ASSERT_FALSE(pages[0].empty());
  EXPECT_EQ(pages[0][0].x, 28);
}
TEST(WrenLayout, ZonePreventsUnneededHyphen) {
  auto spec = wrenSpec();
  EXPECT_TRUE(hyphenated(layout(WORDS, spec)[0][0]));
  spec.hyphenationZone = 10;
  EXPECT_FALSE(hyphenated(layout(WORDS, spec)[0][0]));
}
TEST(WrenLayout, MinimumWordLetters) {
  auto spec = wrenSpec();
  spec.hyphenationMinWordLetters = 13;
  EXPECT_FALSE(hyphenated(layout(WORDS, spec)[0][0]));
}
TEST(WrenLayout, MinimumLeftLetters) {
  auto spec = wrenSpec();
  spec.hyphenationMinLeft = 7;
  EXPECT_FALSE(hyphenated(layout(WORDS, spec)[0][0]));
}
TEST(WrenLayout, MinimumRightLetters) {
  auto spec = wrenSpec();
  spec.hyphenationMinRight = 7;
  EXPECT_FALSE(hyphenated(layout(WORDS, spec)[0][0]));
}
TEST(WrenLayout, ConsecutiveLimitAndPageEnd) {
  auto spec = wrenSpec();
  spec.hyphenationMaxConsecutive = 1;
  const auto pages = layout(WORDS, spec);
  for (const auto& page : pages) {
    ASSERT_FALSE(page.empty());
    EXPECT_FALSE(hyphenated(page.back()));
    for (size_t i = 1; i < page.size(); ++i) EXPECT_FALSE(hyphenated(page[i - 1]) && hyphenated(page[i]));
  }
}
TEST(WrenLayout, SingleLinePagesNeverEndWithDiscretionaryHyphen) {
  for (const auto& page : layout(WORDS, wrenSpec(), 100, 20)) {
    ASSERT_EQ(page.size(), 1u);
    EXPECT_FALSE(hyphenated(page[0]));
  }
}
TEST(WrenLayout, SoftHyphenIsInvisibleExceptAsciiBreak) {
  const auto pages = layout(WORDS, wrenSpec());
  EXPECT_TRUE(hyphenated(pages[0][0]));
  for (const auto& page : pages)
    for (const auto& line : page) EXPECT_EQ(line.text.find("\xC2\xAD"), std::string::npos);
  EXPECT_FALSE(hyphenated(pages.back().back()));
}
TEST(WrenLayout, UkrainianApostropheRemainsInsideWord) {
  for (const char* word : {"об'єднання", "об’єднання"}) {
    const auto pages = layout(word, wrenSpec(), 300);
    ASSERT_EQ(pages[0].size(), 1u);
    EXPECT_EQ(pages[0][0].text, "об’єднання");
  }
}

TEST(WrenLayout, LargeParagraphGapRoundsHalfLineUp) {
  auto spec = wrenSpec();
  spec.paragraphSpaceLines = 50;
  const auto pages = layout("one", spec, 100, 105, true);
  ASSERT_EQ(pages[0].size(), 2u);
  EXPECT_EQ(pages[0][1].y - pages[0][0].y, 40);
  EXPECT_EQ(pages[0][0].x, 0);
  EXPECT_EQ(pages[0][1].x, 0);
}
TEST(WrenLayout, ZeroOverridesUseUpstreamPitch) {
  const auto pages = layout("one two three four five six seven eight nine", ReaderRenderSpec{});
  ASSERT_GE(pages[0].size(), 2u);
  EXPECT_EQ(pages[0][0].y, 0);
  EXPECT_NE(pages[0][1].y - pages[0][0].y, 20);
}
TEST(WrenLayout, EnglishLanguageTagsIncludeIsoAndCaseVariants) {
  for (const auto* tag : {"en", "EN-us", "eng", "ENG", "en_GB"}) EXPECT_TRUE(wren::isEnglishLanguage(tag));
  for (const auto* tag : {"", "e", "ru", "uk", "end"}) EXPECT_FALSE(wren::isEnglishLanguage(tag));
}

TEST(WrenLayout, EveryOverrideChangesLayoutSignature) {
  ReaderRenderSpec spec;
  const auto original = readerRenderSpecSignature(spec);
  const auto changed = [&](auto member) {
    auto copy = spec;
    copy.*member = 1;
    EXPECT_NE(readerRenderSpecSignature(copy), original);
  };
  changed(&ReaderRenderSpec::linePitchPx);
  changed(&ReaderRenderSpec::fontEmHundredths);
  changed(&ReaderRenderSpec::paragraphIndentEm);
  changed(&ReaderRenderSpec::paragraphSpaceLines);
  changed(&ReaderRenderSpec::hyphenationZone);
  changed(&ReaderRenderSpec::hyphenationMinWordLetters);
  changed(&ReaderRenderSpec::hyphenationMinLeft);
  changed(&ReaderRenderSpec::hyphenationMinRight);
  changed(&ReaderRenderSpec::hyphenationMaxConsecutive);
}

TEST(WrenLayout, ApostrophesDoNotCountAsLettersForMinimumWord) {
  for (const auto* word : {"aa об'єд\xC2\xADнання aa", "aa об’єд\xC2\xADнання aa"}) {
    auto spec = wrenSpec();
    spec.hyphenationMinWordLetters = 9;
    EXPECT_TRUE(hyphenated(layout(word, spec, 96, 85, false, "uk")[0][0]));
    spec.hyphenationMinWordLetters = 10;
    EXPECT_FALSE(hyphenated(layout(word, spec, 96, 85, false, "uk")[0][0]));
  }
}
TEST(WrenLayout, HeadingIndentFollowsBookLanguage) {
  auto spec = wrenSpec();
  spec.paragraphIndentEm = 100;
  spec.fontEmHundredths = 2800;
  const char* body = "</p><h1>Title</h1><p>body words";
  const auto en = layout(body, spec, 180, 200, false, "en");
  const auto ru = layout(body, spec, 180, 200, false, "ru");
  ASSERT_GE(en[0].size(), 2u);
  ASSERT_GE(ru[0].size(), 2u);
  EXPECT_EQ(en[0].back().x, 0);
  EXPECT_EQ(ru[0].back().x, 28);
}
TEST(WrenLayout, SceneBreakSuppressesEnglishIndent) {
  auto spec = wrenSpec();
  spec.paragraphIndentEm = 100;
  spec.fontEmHundredths = 2800;
  const auto pages = layout("one</p><hr/><p>two", spec, 180, 200);
  ASSERT_GE(pages[0].size(), 2u);
  EXPECT_EQ(pages[0].back().x, 0);
}
TEST(WrenLayout, BookIndentYieldsToMode) {
  auto spec = wrenSpec();
  spec.paragraphIndentEm = 100;
  spec.fontEmHundredths = 2800;
  const auto pages = layout("</p><p style=\"text-indent: -40px\">one", spec, 180, 200);
  ASSERT_FALSE(pages[0].empty());
  EXPECT_EQ(pages[0].back().x, 28);
}
TEST(WrenLayout, LargeInlineAndDropCapKeepBodyGrid) {
  const auto pages =
      layout("<span style=\"float:left;font-size:3em\">A</span> one two three four five six seven", wrenSpec());
  ASSERT_FALSE(pages[0].empty());
  EXPECT_NE(pages[0][0].text.find("A"), std::string::npos);
  for (const auto& page : pages)
    for (size_t i = 1; i < page.size(); ++i) EXPECT_EQ(page[i].y - page[i - 1].y, 20);
}
TEST(WrenLayout, TableCellsUsePageSafeStackedFlow) {
  const auto pages =
      layout("</p><table><tr><td>aa abcdef\xC2\xADghijkl</td><td>next cell words</td></tr></table><p>end", wrenSpec(),
             100, 40);
  ASSERT_GE(pages.size(), 2u);
  for (const auto& page : pages)
    if (!page.empty()) EXPECT_FALSE(hyphenated(page.back()));
}

TEST(WrenLayout, SoftFlushPreservesConsecutiveLimit) {
  GfxRenderer renderer;
  BlockStyle style;
  style.alignment = CssTextAlign::Left;
  ParsedText text(true, false, style, 0);
  auto spec = wrenSpec();
  spec.hyphenationMaxConsecutive = 1;
  text.setWrenTypography(spec);
  Hyphenator::setPreferredLanguage("en");
  bool previousHyphen = false;
  size_t hyphens = 0;
  const auto emit = [&](std::unique_ptr<TextBlock> line, auto) {
    const std::string last = line->wordText(line->wordCount() - 1);
    const bool hyphen = !last.empty() && last.back() == '-';
    EXPECT_FALSE(previousHyphen && hyphen);
    previousHyphen = hyphen;
    if (hyphen) ++hyphens;
  };
  for (int chunk = 0; chunk < 20; ++chunk) {
    for (int i = 0; i < 5; ++i) {
      text.addWord("aa", EpdFontFamily::REGULAR);
      text.addWord("abcdef\xC2\xADghijkl", EpdFontFamily::REGULAR);
    }
    text.layoutAndExtractLines(renderer, 0, 100, emit, false);
  }
  text.layoutAndExtractLines(renderer, 0, 100, emit);
  EXPECT_GT(hyphens, 0u);
}

TEST(WrenLayout, RubyConsumesWholePitchSlotsAndPreservesPageEnd) {
  const auto pages =
      layout("one <ruby>base<rt>annotation</rt></ruby> two three four five six seven eight nine", wrenSpec(), 100, 85);
  bool extraSlot = false;
  for (const auto& page : pages) {
    ASSERT_FALSE(page.empty());
    EXPECT_FALSE(hyphenated(page.back()));
    for (size_t i = 1; i < page.size(); ++i) {
      const int advance = page[i].y - page[i - 1].y;
      EXPECT_EQ(advance % 20, 0);
      if (advance > 20) extraSlot = true;
    }
  }
  EXPECT_TRUE(extraSlot);
  ASSERT_GE(pages[0].size(), 3u);
  for (size_t i = 1; i < pages[0].size(); ++i) {
    const bool rubyLine = pages[0][i - 1].text.find("base") != std::string::npos;
    EXPECT_EQ(pages[0][i].y - pages[0][i - 1].y, rubyLine ? 40 : 20);
  }
}

TEST(WrenLayout, LastOrdinaryLineFitsGlyphOverhang) {
  const auto pages =
      layout("one two three four five six seven eight nine ten eleven twelve", wrenSpec(), 100, 85, false, "en", 25);
  ASSERT_FALSE(pages.empty());
  EXPECT_EQ(pages[0][0].y, 0);
  for (const auto& page : pages) {
    for (size_t i = 1; i < page.size(); ++i) EXPECT_EQ(page[i].y - page[i - 1].y, 20);
    EXPECT_LE(page.back().y + 25, 85);
  }
}

TEST(WrenLayout, GluedTitleNeverSplitsFromName) {
  const auto pages = layout("long Dr. Watson end", wrenSpec(), 88, 200, false, "en");
  bool found = false;
  for (const auto& page : pages) {
    for (const auto& line : page) {
      if (line.text.find("Dr.") != std::string::npos) {
        found = true;
        EXPECT_NE(line.text.find("Watson"), std::string::npos);
      }
    }
  }
  EXPECT_TRUE(found);
}
TEST(WrenLayout, RussianDialogueAndInlineNestedQuotes) {
  const auto pages =
      layout("- Да, - ответил он. <i>\"Он</i> сказал: \"привет\"\" ...", wrenSpec(), 600, 200, false, "ru");
  std::string text;
  for (const auto& page : pages)
    for (const auto& line : page) text += line.text;
  EXPECT_NE(text.find("«Он"), std::string::npos);
  EXPECT_NE(text.find("„привет“»"), std::string::npos);
  EXPECT_NE(text.find("…"), std::string::npos);
  EXPECT_EQ(text.find(" - "), std::string::npos);
}
