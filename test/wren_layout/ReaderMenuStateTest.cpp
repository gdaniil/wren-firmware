#include <gtest/gtest.h>

#include "src/wren/reader/ReaderMenuState.h"

TEST(ReaderMenuState, OrientationAndScroll) {
  using namespace wren::reader;
  EXPECT_EQ(toggleOrientation(0), 1);
  EXPECT_EQ(toggleOrientation(2), 1);
  EXPECT_EQ(toggleOrientation(1), 0);
  EXPECT_EQ(toggleOrientation(3), 0);
  EXPECT_EQ(scrollStart(4, 0, 3), 2);
  EXPECT_EQ(scrollStart(1, 2, 3), 1);
  EXPECT_TRUE(within(5, UINT32_MAX - 4, 11));
  EXPECT_FALSE(within(6, UINT32_MAX - 4, 11));
}
