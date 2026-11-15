#include <gtest/gtest.h>
#include "src/wren/setup/SetupState.h"

TEST(SetupState, OptionsAndViewport) {
  using namespace wren::setup;
  EXPECT_EQ(sleepScreenIndex(3), 1);
  EXPECT_EQ(sleepScreenIndex(0), 0);
  EXPECT_EQ(sleepScreenIndex(4), -1);
  EXPECT_EQ(batteryIndex(1), 1);
  EXPECT_EQ(sleepAfterIndex(10), 1);
  EXPECT_EQ(sleepAfterIndex(15), -1);
  EXPECT_EQ(languageIndex(4, 2, 4), 1);
  EXPECT_EQ(languageIndex(5, 2, 4), -1);
  EXPECT_EQ(displayRowCount(true), 3);
  EXPECT_EQ(scrollStart(0, 5, 3, 6), 3);
  EXPECT_EQ(scrollStart(3, 1, 3, 6), 1);
}
