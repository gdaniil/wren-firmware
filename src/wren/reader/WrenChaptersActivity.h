#pragma once
#include <memory>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"
class Epub;
class WrenChaptersActivity final : public Activity {
 public:
  WrenChaptersActivity(GfxRenderer&, MappedInputManager&, const std::shared_ptr<Epub>&, int);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  void cancel();
  void move(int delta, bool wrap);
  int visible() const;
  std::shared_ptr<Epub> epub;
  int selected, start = 0;
  bool first = true;
  ButtonNavigator navigator{250, 500};
};
