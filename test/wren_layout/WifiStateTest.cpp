#include <gtest/gtest.h>

#include "src/wren/send/WifiState.h"

using namespace wren::wifi;

TEST(WifiState, SignalAndSubtitle) {
  EXPECT_EQ(signalBars(-50), 4);
  EXPECT_EQ(signalBars(-51), 3);
  EXPECT_EQ(signalBars(-60), 3);
  EXPECT_EQ(signalBars(-61), 2);
  EXPECT_EQ(signalBars(-70), 2);
  EXPECT_EQ(signalBars(-71), 1);
  EXPECT_EQ(subtitle(true, true), Subtitle::Saved);
  EXPECT_EQ(subtitle(false, true), Subtitle::Secured);
  EXPECT_EQ(subtitle(false, false), Subtitle::Open);
}

TEST(WifiState, RowsAndNavigation) {
  EXPECT_TRUE(noNetworks(0));
  EXPECT_FALSE(noNetworks(2));
  EXPECT_EQ(rowCount(0), 2u);
  EXPECT_EQ(row(0, 0), Row::ScanAgain);
  EXPECT_EQ(row(1, 0), Row::HiddenNetwork);
  EXPECT_EQ(row(1, 2), Row::Network);
  EXPECT_EQ(row(2, 2), Row::ScanAgain);
  EXPECT_EQ(row(3, 2), Row::HiddenNetwork);
  EXPECT_EQ(move(0, 4, -1), 3u);
  EXPECT_EQ(move(3, 4, 1), 0u);
  EXPECT_EQ(scrollStart(0, 3, 2, 4), 2u);
  EXPECT_EQ(scrollStart(2, 0, 2, 4), 0u);
}

TEST(WifiState, PromptDefaults) {
  EXPECT_EQ(defaultRow(Prompt::Save), 0u);
  EXPECT_EQ(defaultRow(Prompt::Failed), 0u);
  EXPECT_EQ(defaultRow(Prompt::Forget), 0u);
  EXPECT_EQ(defaultRow(Prompt::NoneFound), 0u);
}

TEST(WifiState, FailureCause) {
  EXPECT_EQ(failureCause(false, false), Failure::General);
  EXPECT_EQ(failureCause(true, false), Failure::NoSsid);
  EXPECT_EQ(failureCause(false, true), Failure::Timeout);
}
