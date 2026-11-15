#include <gtest/gtest.h>

#include "src/wren/ui/KeyboardState.h"

using namespace wren::ui::keyboard;

TEST(KeyboardState, RowsFitAvailableWidth) {
  for (const int count : {10, 12}) {
    EXPECT_EQ(keyLeft(488, count, 4, 0), 0);
    EXPECT_EQ(keyLeft(488, count, 4, count - 1) + keyWidth(488, count, 4, count - 1), 488);
    for (int i = 1; i < count; ++i)
      EXPECT_EQ(keyLeft(488, count, 4, i) - keyLeft(488, count, 4, i - 1) - keyWidth(488, count, 4, i - 1), 4);
  }
}

TEST(KeyboardState, SelectionAndPasswordSwitch) {
  const int counts[] = {10, 12, 6};
  Selection selection;
  selection.column = 5;
  selection.moveRow(1, 3, counts, true);
  EXPECT_EQ(selection.column, 6);
  selection.moveRow(-1, 3, counts, true);
  selection.moveRow(-1, 3, counts, true);
  EXPECT_EQ(selection.focus, Focus::PasswordSwitch);
  selection.moveRow(1, 3, counts, true);
  EXPECT_EQ(selection.focus, Focus::Keys);
  selection.moveRow(-1, 3, counts, false);
  EXPECT_EQ(selection.row, 2);
}

TEST(KeyboardState, CaretScroll) {
  EXPECT_EQ(scrollOffset(0, 120, 3, 100), 23);
  EXPECT_EQ(scrollOffset(23, 10, 3, 100), 10);
  EXPECT_EQ(scrollOffset(10, 50, 3, 100), 10);
}

TEST(KeyboardState, NextLanguageLabel) {
  EXPECT_STREQ(languageLabel(LanguageKey::Ru, true), "ru");
  EXPECT_STREQ(languageLabel(LanguageKey::Uk, true), "ua");
  EXPECT_STREQ(languageLabel(LanguageKey::En, true), "en");
  EXPECT_STREQ(languageLabel(LanguageKey::Uk, false), "");
}
