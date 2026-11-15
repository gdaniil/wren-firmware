#include <gtest/gtest.h>

#include "src/wren/home/HomeState.h"

TEST(HomeState, SortRowsSkipPinnedAndContinueBooks) {
  const uint16_t skipped[] = {1, 2, 3};
  EXPECT_EQ(wren::home::sortRowFor(0, skipped, 3, 5), 0);
  EXPECT_EQ(wren::home::sortRowFor(1, skipped, 3, 5), 4);
  EXPECT_EQ(wren::home::sortRowFor(2, skipped, 3, 5), wren::home::NO_BOOK);
  EXPECT_EQ(wren::home::sortRowFor(2, nullptr, 0, 5), 2);
}

TEST(HomeState, FocusAndScrollClamp) {
  EXPECT_EQ(wren::home::moveFocus(0, -1, 4), 0);
  EXPECT_EQ(wren::home::moveFocus(3, 1, 4), 3);
  EXPECT_EQ(wren::home::moveFocus(0, 1, 0), -1);
  EXPECT_EQ(wren::home::scrollStart(0, 4, 3, 8), 2);
  EXPECT_EQ(wren::home::scrollStart(3, 1, 3, 8), 1);
  EXPECT_EQ(wren::home::scrollStart(7, -1, 3, 8), 5);
}

TEST(HomeState, ScreenAndPresentationChoices) {
  using wren::home::ScreenState;
  EXPECT_EQ(wren::home::screenState(true, 0, false), ScreenState::Scanning);
  EXPECT_EQ(wren::home::screenState(false, 0, false), ScreenState::Empty);
  EXPECT_EQ(wren::home::screenState(false, 0, true), ScreenState::Search);
  EXPECT_EQ(wren::home::screenState(false, 1, false), ScreenState::Library);
  EXPECT_EQ(wren::home::titleHash(""), 2166136261u);
  EXPECT_LT(wren::home::coverPattern("book"), 5u);
  EXPECT_FALSE(wren::home::useSmallTabFont(92, 100));
  EXPECT_TRUE(wren::home::useSmallTabFont(93, 100));
}
