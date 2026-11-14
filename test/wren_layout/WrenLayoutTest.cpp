#include <gtest/gtest.h>

#include "src/wren/ui/WrenLayout.h"

using namespace wren::ui::layout;
TEST(WrenLayout, TabPositions) {
  EXPECT_EQ(tabX(528, 0), 38);
  EXPECT_EQ(tabX(528, 3), 384);
  EXPECT_EQ(tabX(480, 0), 25);
  EXPECT_EQ(tabX(480, 3), 350);
  EXPECT_EQ(tabX(480, 4), 0);
}
TEST(WrenLayout, AvailableSpace) {
  EXPECT_EQ(rowCount(28, 136), 2);
  EXPECT_EQ(rowCount(50, 49), 0);
  EXPECT_EQ(truncationWidth(20, 100), 80);
  EXPECT_EQ(truncationWidth(100, 20), 0);
}
