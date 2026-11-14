#pragma once

#include <memory>
#include <string>
#include <vector>

#include "BookmarkEntry.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class Epub;

class WrenBookmarksActivity final : public Activity {
 public:
  WrenBookmarksActivity(GfxRenderer&, MappedInputManager&, const std::shared_ptr<Epub>&, const std::string&);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  struct Row {
    std::string meta;
    std::vector<std::string> lines;
    int height = 0;
  };

  void cancel();
  void openSelectedBookmark();
  void deleteSelectedBookmark();
  void move(int delta);
  void rebuildWindow();
  Row makeRow(int index) const;
  int bodyHeight() const;

  std::shared_ptr<Epub> epub;
  std::string epubPath;
  std::vector<BookmarkEntry> bookmarks;
  std::vector<Row> rows;
  std::vector<int> heights;
  int selected = 0;
  int start = 0;
  int font = 0;
  int lineHeight = 0;
  bool first = true;
  ButtonNavigator navigator{250, 500};
};
