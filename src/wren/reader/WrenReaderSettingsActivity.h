#pragma once
#include <string>
#include <vector>

#include "ReaderSettingsState.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class WrenReaderSettingsActivity final : public Activity {
 public:
  // liveRotation: rotate the screen when Orientation changes (inside a book). From Setup it only saves the setting.
  WrenReaderSettingsActivity(GfxRenderer&, MappedInputManager&, const std::string& title, bool liveRotation = true);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  void close();
  void change(int delta);
  void updatePreview();
  std::string title;
  std::vector<std::string> lines;
  int selected = 0;
  int font = 0;
  bool liveRotation = true;
  bool changed = false, cleanRefresh = true;
  ButtonNavigator navigator{250, 500};
};
