#pragma once
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class WrenSetupSystemActivity final : public Activity {
 public:
  WrenSetupSystemActivity(GfxRenderer& r, MappedInputManager& i) : Activity("WrenSystem", r, i) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  void drawPage();
  void confirmClearCache();
  void change(int delta);
  void close();
  int selected = 0;
  int start = 0;
  bool fullRefresh = true;
  bool changed = false;
  ButtonNavigator navigator{250, 500};
};
