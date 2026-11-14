#pragma once
#include "activities/Activity.h"
#include "activities/reader/EpubReaderMenuActivity.h"

class WrenReaderMenuActivity final : public Activity {
 public:
  using Action = EpubReaderMenuActivity::MenuAction;
  WrenReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& input, const std::string& title, int page,
                         int total, int percent, uint8_t orientation, size_t bookmarks, Action& selected, bool added);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool handleHomeGesture() override;

 private:
  void close(bool cancelled);
  Action& selectedAction;
  std::string title;
  int page, total, percent;
  uint8_t orientation;
  size_t bookmarks;
  int selected = 0, start = 0;
  bool added, confirming = false;
  uint32_t addedAt = 0, confirmAt = 0;
};
