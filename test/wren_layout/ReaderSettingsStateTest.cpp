#include <gtest/gtest.h>

#include "src/wren/reader/ReaderSettingsState.h"

TEST(ReaderSettingsState, CyclesBothDirections) {
  using wren::reader::cycleSetting;
  EXPECT_EQ(cycleSetting(0, -1, 3), 2);
  EXPECT_EQ(cycleSetting(2, 1, 3), 0);
  EXPECT_EQ(cycleSetting(0, -1, 2), 1);
  EXPECT_EQ(cycleSetting(1, 1, 2), 0);
}
TEST(ReaderSettingsState, DistributesExtraPixels) {
  using wren::reader::extraSpace;
  EXPECT_EQ(extraSpace(8, 3, 0), 3);
  EXPECT_EQ(extraSpace(8, 3, 1), 3);
  EXPECT_EQ(extraSpace(8, 3, 2), 2);
  EXPECT_EQ(extraSpace(-1, 3, 0), 0);
  EXPECT_EQ(extraSpace(8, 0, 0), 0);
}
