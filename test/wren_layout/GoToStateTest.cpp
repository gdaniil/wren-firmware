#include <gtest/gtest.h>

#include "src/wren/reader/GoToState.h"
TEST(GoToState, ClampsSteps) {
  EXPECT_EQ(wren::reader::stepPercent(99, 10), 100);
  EXPECT_EQ(wren::reader::stepPercent(1, -10), 0);
}
TEST(GoToState, MapsLikeReaderJump) {
  auto cumulative = [](int i) { return i == 0 ? 50u : 100u; };
  EXPECT_EQ(wren::reader::percentSpine(0, 100, 2, cumulative), 0);
  EXPECT_EQ(wren::reader::percentSpine(50, 100, 2, cumulative), 0);
  EXPECT_EQ(wren::reader::percentSpine(51, 100, 2, cumulative), 1);
  EXPECT_EQ(wren::reader::percentSpine(100, 100, 2, cumulative), 1);
  EXPECT_EQ(wren::reader::percentSpine(0, 0, 2, cumulative), -1);
}
