#include <gtest/gtest.h>

#include <limits>

#include "src/wren/reader/BookmarksState.h"

TEST(BookmarksState, MetaFormatsPercentChapterAndName) {
  char text[100];
  wren::reader::bookmarkMeta(text, sizeof(text), "%d%% · ch. %d", 0.336f, 4, "note");
  EXPECT_STREQ(text, "34% · ch. 5 · note");
  wren::reader::bookmarkMeta(text, sizeof(text), "%d%% · ch. %d", 2.0f, -1, "");
  EXPECT_STREQ(text, "100%");
  wren::reader::bookmarkMeta(text, sizeof(text), "%d%% · ch. %d", -0.5f, -1, "name");
  EXPECT_STREQ(text, "0% · name");
  EXPECT_EQ(wren::reader::bookmarkPercent(std::numeric_limits<float>::quiet_NaN()), 0);
}

TEST(BookmarksState, ScrollsWholeVariableHeightEntries) {
  const int heights[] = {40, 70, 50, 80};
  EXPECT_EQ(wren::reader::bookmarkScrollStart(heights, 4, 2, 0, 120), 1);
  EXPECT_EQ(wren::reader::bookmarkScrollStart(heights, 4, 3, 1, 120), 3);
  EXPECT_EQ(wren::reader::bookmarkScrollStart(heights, 4, 0, 3, 120), 0);
  EXPECT_EQ(wren::reader::bookmarkScrollStart(nullptr, 0, 0, 0, 120), 0);
  EXPECT_EQ(wren::reader::bookmarkScrollStart(heights, 4, 3, 0, 20), 3);
}
