#pragma once
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class WrenSetupDisplayActivity final : public Activity {
 public:
  WrenSetupDisplayActivity(GfxRenderer& r, MappedInputManager& i) : Activity("WrenDisplay", r, i) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  void change(int delta);
  void close();
  int selected = 0;
  int start = 0;
  bool fullRefresh = true;
  bool changed = false;
  ButtonNavigator navigator{250, 500};
};
