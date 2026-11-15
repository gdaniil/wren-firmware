#pragma once

#include "activities/Activity.h"

class WrenNoCardActivity final : public Activity {
 public:
  WrenNoCardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("NoCard", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  bool handleHomeGesture() override { return true; }
  void render(RenderLock&&) override;
};
