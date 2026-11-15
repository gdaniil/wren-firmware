#pragma once
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class WrenFilesActivity final : public Activity {
 public:
  WrenFilesActivity(GfxRenderer& r, MappedInputManager& i, const std::string& path = "/");
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void drawPage();
  void loadFiles();
  void updateWindow();
  void openSelected();
  void up();
  void deleteSelected();
  bool isFile() const;
  int count() const;
  int visibleRows() const;
  std::string selectedPath() const;
  const std::string& entryAt(int row) const;
  std::string path;
  std::vector<std::string> entries;
  // Current folder names are the only unbounded data. The metadata cache owns
  // one uint64_t per visible row, reused until the window/folder changes.
  std::vector<uint64_t> values;
  char nameBuffer[512]{};
  int selected = 0;
  int start = 0;
  int cachedStart = -1;
  int cachedVisible = -1;
  bool cleanRefresh = true;
  ButtonNavigator navigator{250, 500};
};
