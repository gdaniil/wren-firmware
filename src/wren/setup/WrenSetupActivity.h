#pragma once
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class WrenSetupActivity final : public Activity {
 public:
  WrenSetupActivity(GfxRenderer& r, MappedInputManager& i) : Activity("WrenSetup", r, i) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  void open();
  int selected = 0;
  int start = 0;
  bool fullRefresh = true;
  ButtonNavigator navigator{250, 500};
};
