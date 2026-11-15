#include <gtest/gtest.h>

#include <algorithm>
#include <array>

#include "src/wren/setup/FilesState.h"

TEST(FilesState, DirectoriesBeforeFilesAlphabetically) {
  std::array<std::string_view, 5> names = {"Z.epub", "Zoo/", "a.epub", "books/", "B.txt"};
  std::sort(names.begin(), names.end(), wren::files::less);
  EXPECT_EQ(names[0], "books/");
  EXPECT_EQ(names[1], "Zoo/");
  EXPECT_EQ(names[2], "a.epub");
  EXPECT_EQ(names[3], "B.txt");
  EXPECT_EQ(names[4], "Z.epub");
}
TEST(FilesState, SizeUnitsAndRounding) {
  char size[32];
  wren::files::formatSize(size, sizeof(size), 0);
  EXPECT_STREQ(size, "0 kb");
  wren::files::formatSize(size, sizeof(size), 1024);
  EXPECT_STREQ(size, "1 kb");
  wren::files::formatSize(size, sizeof(size), 104857);
  EXPECT_STREQ(size, "102 kb");
  wren::files::formatSize(size, sizeof(size), 104858);
  EXPECT_STREQ(size, "0.1 mb");
  wren::files::formatSize(size, sizeof(size), 314573);
  EXPECT_STREQ(size, "0.3 mb");
  wren::files::formatSize(size, sizeof(size), 1048576);
  EXPECT_STREQ(size, "1.0 mb");
}
TEST(FilesState, TailPreservesUtf8AndScrollClampsAfterDelete) {
  EXPECT_EQ(wren::files::pathTail("/books/title.epub", 10), "title.epub");
  EXPECT_EQ(wren::files::pathTail("/я.txt", 5), ".txt");
  EXPECT_EQ(wren::files::pathTail("/", 0), "");
  EXPECT_EQ(wren::files::clampSelection(8, 3), 2);
  EXPECT_EQ(wren::files::clampSelection(3, 0), 0);
  EXPECT_EQ(wren::files::scrollStart(0, 8, 4, 10), 5);
  EXPECT_EQ(wren::files::scrollStart(5, 1, 4, 10), 1);
}
TEST(FilesState, IconsAreCaseInsensitive) {
  using namespace wren::files;
  EXPECT_EQ(iconKind("folder/"), IconKind::Folder);
  EXPECT_EQ(iconKind("cover.BMP"), IconKind::Image);
  EXPECT_EQ(iconKind("BOOK.EPUB"), IconKind::Book);
  EXPECT_EQ(iconKind("unknown.bin"), IconKind::Book);
}
