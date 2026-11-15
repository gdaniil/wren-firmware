#include <gtest/gtest.h>

#include "src/wren/send/SendState.h"

using namespace wren::send;

TEST(SendState, SsidAndMenu) {
  const uint8_t mac[] = {0, 1, 2, 3, 0x3f, 0x2a};
  char ssid[10];
  formatSsid(ssid, mac);
  EXPECT_STREQ(ssid, "wren-3f2a");
  EXPECT_EQ(menuRowCount(false), 3);
  EXPECT_EQ(menuRowCount(true), 4);
  EXPECT_EQ(menuRow(3), MenuRow::UsbDrive);
}

TEST(SendState, HotspotTransitions) {
  HotspotSteps steps;
  EXPECT_TRUE(steps.poll(1, false, false));
  EXPECT_FALSE(steps.poll(0, false, false));
  EXPECT_TRUE(steps.poll(0, false, false));
  EXPECT_TRUE(steps.poll(0, false, true));
  EXPECT_FALSE(steps.poll(0, true, false));
  EXPECT_FALSE(steps.poll(0, true, false));
  EXPECT_EQ(steps.step, HotspotStep::Open);
}

TEST(SendState, ReceivedList) {
  ReceivedList list;
  list.add("one", 1);
  list.add("two", 2);
  list.add("three", 3);
  list.add("four", 4);
  EXPECT_EQ(list.count, 4u);
  EXPECT_EQ(list.visibleCount(), 3u);
  EXPECT_STREQ(list.files[0].name, "four");
  EXPECT_STREQ(list.files[2].name, "two");
  char longName[200];
  memset(longName, 'a', sizeof(longName));
  longName[199] = '\0';
  list.add(longName, 5);
  EXPECT_EQ(strlen(list.files[0].name), 95u);
}

TEST(SendState, ProgressAndViewport) {
  EXPECT_EQ(percent(1, 0), 0u);
  EXPECT_EQ(percent(48, 100), 48u);
  EXPECT_EQ(percent(101, 100), 100u);
  UploadRedraw redraw;
  EXPECT_FALSE(redraw.update(true, true, 0, 1000));
  EXPECT_TRUE(redraw.update(true, false, 25, 2000));
  EXPECT_FALSE(redraw.update(true, false, 50, 3000));
  EXPECT_TRUE(redraw.update(true, false, 50, 4000));
  EXPECT_TRUE(redraw.update(true, false, 75, 6000));
  EXPECT_TRUE(redraw.update(false, true, 100, 6100));
  EXPECT_EQ(scrollStart(0, 3, 2, 4), 2);
  EXPECT_EQ(scrollStart(2, 0, 2, 4), 0);
}
