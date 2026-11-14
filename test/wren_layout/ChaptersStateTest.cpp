#include <gtest/gtest.h>

#include "src/wren/reader/ChaptersState.h"
TEST(ChaptersState, PercentRoundsAndHandlesEmpty) {
  EXPECT_EQ(wren::reader::chapterPercent(1, 3), 33);
  EXPECT_EQ(wren::reader::chapterPercent(2, 3), 67);
  EXPECT_EQ(wren::reader::chapterPercent(0, 0), 0);
}
TEST(ChaptersState, NumberAndPageJump) {
  char text[16];
  wren::reader::chapterNumber(text, sizeof(text), 0);
  EXPECT_STREQ(text, "01");
  wren::reader::chapterNumber(text, sizeof(text), 99);
  EXPECT_STREQ(text, "100");
  EXPECT_EQ(wren::reader::chapterPageJump(4, 10, 8), 7);
  EXPECT_EQ(wren::reader::chapterPageJump(4, -10, 8), 0);
}
