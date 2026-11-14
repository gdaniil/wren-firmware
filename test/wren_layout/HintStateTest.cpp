#include <gtest/gtest.h>

#include "src/wren/reader/HintState.h"

TEST(HintState, RevealAndTimeout) {
  wren::reader::HintState hints;
  EXPECT_TRUE(hints.reveal(100));
  EXPECT_FALSE(hints.reveal(200));
  EXPECT_FALSE(hints.expire(3099));
  EXPECT_TRUE(hints.expire(3100));
  EXPECT_TRUE(hints.reveal(3101));
  hints.input(4000);
  EXPECT_FALSE(hints.expire(6999));
  EXPECT_TRUE(hints.expire(7000));
}

TEST(HintState, HideAndClockWrap) {
  wren::reader::HintState hints;
  EXPECT_TRUE(hints.reveal(UINT32_MAX - 999));
  EXPECT_FALSE(hints.expire(1999));
  EXPECT_TRUE(hints.expire(2000));
  EXPECT_TRUE(hints.reveal(3000));
  hints.hide();
  EXPECT_FALSE(hints.visible());
  EXPECT_TRUE(hints.reveal(3001));
}
