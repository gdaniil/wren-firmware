#pragma once

#include <LibraryIndexFile.h>

#include <cstdint>
#include <memory>
#include <string>

#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Wren home: the library tab. Continue card, every book on the card, covers.
// Send and setup tabs open the upstream screens until ours exist.
class WrenHomeActivity final : public Activity {
 public:
  WrenHomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool cleanInitialRefresh);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isHomeActivity() const override { return true; }

 private:
  // At most this many rows are ever on screen (portrait, no card: 5).
  static constexpr int MAX_ROWS = 8;
  static constexpr int MAX_FAILED = 16;

  enum class Cover : uint8_t { Unknown, Ready, Pending, Missing };
  struct Book {
    uint16_t ordinal = 0xFFFF;
    std::string path;
    std::string title;
    std::string author;
    std::string thumb;  // cached thumbnail path, valid when cover == Ready or Pending
    int progress = -1;
    Cover cover = Cover::Unknown;
  };

  // Data
  void openIndex();
  void resolveRecents();
  int listCount() const;
  uint16_t ordinalForListRow(int row);
  bool loadBook(uint16_t ordinal, Book& out);
  Book* rowBook(int row);
  void invalidateRows();
  void applySearch();
  Cover coverState(Book& book, int height);
  bool generateCover(Book& book, int height);
  bool generateOneCover();
  bool failed(const std::string& path) const;
  void markFailed(const std::string& path);

  // Layout
  int contentTop() const;
  bool cardVisible() const;
  int rowsTop() const;
  int visibleRows() const;

  // Drawing
  void drawHeader() const;
  void drawCard(int y, bool focused);
  void drawSection(int y);
  void drawRow(int y, Book& book, bool focused);
  void drawCover(int x, int y, int w, int h, Book& book, bool ink, bool plate);
  void drawEmpty(int top);
  void drawHelp(int y);
  void drawHints();
  void drawMenu();
  void drawScanning();
  void drawPage();

  // Input
  void openFocused();
  void openSearch();
  void openMenu();
  void menuAction();
  void deleteBook(const std::string& path, const std::string& title);
  void move(int delta);

  library::LibraryIndexFile index;
  const bool cleanInitialRefresh;
  bool firstPaint = true;
  bool painted = false;         // a frame is on screen since the last cover was made
  bool swallowConfirm = false;  // Confirm was held on entry; ignore its release
  ButtonNavigator navigator;

  // Continue book (recents[0]) and the other recents, as RecentDesc sort rows.
  bool hasContinue = false;
  Book continueBook;
  int continueChapter = 0;  // 1-based, 0 = unknown
  uint16_t pinnedRows[RecentBooksStore::MAX_RECENT_BOOKS] = {};
  uint8_t pinnedCount = 0;
  // Sort rows of every book shown out of RecentDesc order, ascending.
  uint16_t skippedRows[RecentBooksStore::MAX_RECENT_BOOKS] = {};
  uint8_t skippedCount = 0;
  bool anyOpened = false;

  // Search: one sort row per match. Fallible, at most 2 bytes per indexed book, freed on exit or clear.
  std::string query;
  std::unique_ptr<uint16_t[]> matches;
  uint16_t matchCount = 0;

  // Focus: -1 = continue card, 0.. = list row. start = first list row on screen.
  int focus = 0;
  int start = 0;

  // Visible window cache, keyed by list row.
  Book rows[MAX_ROWS];
  int rowsFirst = -1;

  // Paths whose cover generation failed this session (hash).
  uint32_t failedCovers[MAX_FAILED] = {};
  uint8_t failedCount = 0;

  // Book menu overlay.
  bool menuOpen = false;
  int menuFocus = 0;
};
